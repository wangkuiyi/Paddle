
#include <majel/gpu/detail/cuda.h>
#include <majel/malloc/cpu/system_allocator.h>

#include <majel/contrib/support/knobs.h>

namespace majel {
namespace malloc {
namespace cpu {

class Allocator {
public:
  virtual ~Allocator() {}

public:
  virtual void* malloc(size_t) = 0;
  virtual void free(void*, size_t) = 0;
};

class PinnedAllocator : public Allocator {
public:
  virtual void* malloc(size_t size) {
    return gpu::detail::malloc_pinned_host_memory(size);
  }

  virtual void free(void* address, size_t size) {
    return gpu::detail::free_pinned_host_memory(address, size);
  }
};

class DefaultAllocator : public Allocator {
public:
  virtual void* malloc(size_t size) { return std::malloc(size); }

  virtual void free(void* address, size_t size) { return std::free(address); }
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

  // add the pinned allocator
  if (support::get_knob_value("MAJEL_USE_PINNED_ALLOCATOR", false)) {
    system_allocators.push_back(
        std::unique_ptr<Allocator>(new PinnedAllocator));
  }

  // add the default allocator
  system_allocators.push_back(std::unique_ptr<Allocator>(new DefaultAllocator));
}

void SystemAllocator::shutdown() {
  assert(!system_allocators.empty());

  // destroy all allocators
  system_allocators.clear();
}

bool SystemAllocator::uses_gpu() { return false; }
}
}
}
