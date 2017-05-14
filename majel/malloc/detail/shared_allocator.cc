
#include <majel/malloc/detail/shared_allocator.h>

#include <majel/malloc/detail/buddy_allocator.h>
#include <majel/malloc/detail/parameters.h>

#include <majel/malloc/cpu/system_allocator.h>
#include <majel/malloc/gpu/system_allocator.h>

#include <majel/gpu/detail/cuda.h>

#include <majel/contrib/support/knobs.h>

#include <sstream>
#include <stdexcept>

namespace majel {
namespace malloc {
namespace detail {

typedef std::unique_ptr<BuddyAllocator<gpu::SystemAllocator>>
    GpuAllocatorUniquePtr;
typedef std::vector<GpuAllocatorUniquePtr> GpuAllocatorUniquePtrVector;

class SharedAllocatorMallocVisitor : public boost::static_visitor<void*> {
public:
  SharedAllocatorMallocVisitor(BuddyAllocator<cpu::SystemAllocator>& c,
                               GpuAllocatorUniquePtrVector& g,
                               size_t s)
      : cpu_allocator_(c), gpu_allocators_(g), size_(s) {}

public:
  void* operator()(majel::CpuPlace p) { return cpu_allocator_.malloc(size_); }

  void* operator()(majel::GpuPlace p) {
    if ((size_t)p.device >= gpu_allocators_.size()) {
      std::stringstream stream;

      stream << "No GPU allocator for device " << p.device << "\n";

      throw std::runtime_error(stream.str());
    }

    majel::gpu::detail::set_device(p.device);

    void* address = gpu_allocators_[p.device]->malloc(size_);

    return address;
  }

private:
  BuddyAllocator<cpu::SystemAllocator>& cpu_allocator_;
  GpuAllocatorUniquePtrVector& gpu_allocators_;

private:
  size_t size_;
};

class SharedAllocatorFreeVisitor : public boost::static_visitor<void> {
public:
  SharedAllocatorFreeVisitor(BuddyAllocator<cpu::SystemAllocator>& c,
                             GpuAllocatorUniquePtrVector& g,
                             void* a)
      : cpu_allocator_(c), gpu_allocators_(g), address_(a) {}

public:
  void operator()(majel::CpuPlace p) { return cpu_allocator_.free(address_); }

  void operator()(majel::GpuPlace p) {
    assert((size_t)p.device < gpu_allocators_.size());

    majel::gpu::detail::set_device(p.device);

    gpu_allocators_[p.device]->free(address_);
  }

private:
  BuddyAllocator<cpu::SystemAllocator>& cpu_allocator_;
  GpuAllocatorUniquePtrVector& gpu_allocators_;

private:
  void* address_;
};

class SharedAllocatorMemUsedVisitor : public boost::static_visitor<size_t> {
public:
  SharedAllocatorMemUsedVisitor(BuddyAllocator<cpu::SystemAllocator>& c,
                                GpuAllocatorUniquePtrVector& g)
      : cpu_allocator_(c), gpu_allocators_(g) {}

public:
  size_t operator()(majel::CpuPlace p) { return cpu_allocator_.memory_used(); }

  size_t operator()(majel::GpuPlace p) {
    assert((size_t)p.device < gpu_allocators_.size());

    majel::gpu::detail::set_device(p.device);

    return gpu_allocators_[p.device]->memory_used();
  }

private:
  BuddyAllocator<cpu::SystemAllocator>& cpu_allocator_;
  GpuAllocatorUniquePtrVector& gpu_allocators_;
};

static GpuAllocatorUniquePtrVector gpu_allocators_;
static std::unique_ptr<BuddyAllocator<cpu::SystemAllocator>> cpu_allocator_;

void* SharedAllocator::malloc(majel::Place place, size_t size) {
  SharedAllocatorMallocVisitor visitor(*cpu_allocator_, gpu_allocators_, size);

  return boost::apply_visitor(visitor, place);
}

void SharedAllocator::free(majel::Place place, void* ptr) {
  SharedAllocatorFreeVisitor visitor(*cpu_allocator_, gpu_allocators_, ptr);

  return boost::apply_visitor(visitor, place);
}

size_t SharedAllocator::memory_used(majel::Place place) {
  SharedAllocatorMemUsedVisitor visitor(*cpu_allocator_, gpu_allocators_);

  return boost::apply_visitor(visitor, place);
}

void SharedAllocator::init() {
  gpu_allocators_.resize(majel::gpu::detail::get_device_count());

  int current_device = 0;

  for (auto& allocator : gpu_allocators_) {
    majel::gpu::detail::set_device(current_device++);

    allocator.reset(new BuddyAllocator<gpu::SystemAllocator>(
        Parameters::arena_chunk_size(), Parameters::gpu_system_chunk_size()));
  }

  cpu_allocator_.reset(new BuddyAllocator<cpu::SystemAllocator>(
      Parameters::arena_chunk_size(), Parameters::cpu_system_chunk_size()));
}

void SharedAllocator::shutdown() {
  gpu_allocators_.clear();
  cpu_allocator_.reset(nullptr);
}
}
}
}
