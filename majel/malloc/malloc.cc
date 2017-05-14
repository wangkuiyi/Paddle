
#include <majel/malloc/malloc.h>

#include <majel/malloc/cpu/system_allocator.h>
#include <majel/malloc/gpu/system_allocator.h>

#include <majel/malloc/detail/arena_allocator.h>
#include <majel/malloc/detail/shared_allocator.h>

namespace majel {
namespace malloc {

void init() {
  gpu::SystemAllocator::init();
  cpu::SystemAllocator::init();
  detail::SharedAllocator::init();
}

void shutdown() {
  detail::SharedAllocator::shutdown();
  gpu::SystemAllocator::shutdown();
  cpu::SystemAllocator::shutdown();
}

void* malloc(majel::Place place, size_t size) {
  return detail::ArenaAllocator::malloc(place, size);
}

void free(majel::Place place, void* ptr) {
  return detail::ArenaAllocator::free(place, ptr);
}

size_t memory_used(majel::Place place) {
  return detail::ArenaAllocator::memory_used(place);
}
}
}
