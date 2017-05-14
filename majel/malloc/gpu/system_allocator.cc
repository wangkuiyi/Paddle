#include "majel/malloc/gpu/system_allocator.h"
#include "majel/malloc/detail/parameters.h"

#include "majel/gpu/detail/cuda.h"

#include "majel/contrib/logger/logger.h"

#include <vector>

namespace majel {
namespace malloc {
namespace gpu {

class Allocator {
public:
  virtual ~Allocator() {}

public:
  virtual void* malloc(size_t) = 0;
  virtual void free(void*, size_t) = 0;
};

class DefaultAllocator : public Allocator {
public:
  DefaultAllocator() : total_allocation_size_(0) {}

  virtual void* malloc(size_t size) {
    if (!majel::malloc::detail::Parameters::
            is_allocating_gpu_memory_allowed()) {
      return nullptr;
    }

    // fail if the total size is exceeded
    size_t remaining_size = compute_maximum_allocated_size_();

    if (size > remaining_size) {
      logger::log("GpuDefaultAllocator")
          << "Allocation of size " << size
          << " exceeds remaining GPU memory capacity " << remaining_size
          << ", falling back to other allocators\n";

      return nullptr;
    }

    total_allocation_size_ += size;

    return majel::gpu::detail::malloc(size);
  }

  virtual void free(void* address, size_t size) {
    assert(total_allocation_size_ >= size);

    total_allocation_size_ -= size;

    return majel::gpu::detail::free(address);
  }

private:
  size_t compute_maximum_allocated_size_() const {
    size_t available = 0;
    size_t capacity = 0;

    majel::gpu::detail::get_memory_usage(available, capacity);

    size_t maximum_allocated_size =
        (capacity *
         majel::malloc::detail::Parameters::fraction_of_gpu_memory_to_use());
    size_t buffer = capacity - maximum_allocated_size;

    if (available > buffer) {
      return available - buffer;
    }

    return 0;
  }

private:
  size_t total_allocation_size_;
};

class HostFallbackAllocator : public Allocator {
public:
  HostFallbackAllocator() : total_allocation_size_(0) {}

public:
  virtual void* malloc(size_t size) {
    size_t remaining =
        majel::malloc::detail::Parameters::system_maximum_allocation_size() -
        majel::malloc::detail::Parameters::gpu_maximum_allocation_size() -
        total_allocation_size_;

    if (size > remaining) {
      return nullptr;
    }

    total_allocation_size_ += size;
    return majel::gpu::detail::malloc_pinned_and_mapped_host_memory(size);
  }

  virtual void free(void* address, size_t size) {
    assert(total_allocation_size_ >= size);

    total_allocation_size_ -= size;

    majel::gpu::detail::free_pinned_and_mapped_host_memory(address);
  }

private:
  size_t total_allocation_size_;
};

static std::vector<std::unique_ptr<Allocator>> system_allocators;

void* SystemAllocator::malloc(size_t& index, size_t size) {
  index = 0;

  for (auto& allocator : system_allocators) {
    void* address = allocator->malloc(size);

    if (address == nullptr) {
      ++index;
      continue;
    }

    logger::log("GpuSystemAllocator") << "Allocated " << size
                                      << " bytes at address " << address
                                      << " from allocator " << index << "\n";

    return address;
  }

  return nullptr;
}

void SystemAllocator::free(void* address, size_t size, size_t index) {
  assert(index < system_allocators.size());

  system_allocators[index]->free(address, size);
}

size_t SystemAllocator::index_count() { return system_allocators.size(); }

void SystemAllocator::init() {
  assert(system_allocators.empty());

  // make sure no copies occur
  system_allocators.reserve(2);

  // add the default allocator
  system_allocators.push_back(std::unique_ptr<Allocator>(new DefaultAllocator));

  // add the fallback to host allocator
  system_allocators.push_back(
      std::unique_ptr<Allocator>(new HostFallbackAllocator));

  // Enable the log
  logger::enable_log("GpuDefaultAllocator");
}

void SystemAllocator::shutdown() {
  assert(!system_allocators.empty());

  // destroy all allocators
  system_allocators.clear();
}

bool SystemAllocator::uses_gpu() { return true; }

}  // namespace gpu
}  // namespace malloc
}  // namespace majel
