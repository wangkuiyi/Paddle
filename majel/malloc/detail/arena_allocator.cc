
#include <majel/malloc/detail/arena_allocator.h>
#include <majel/malloc/detail/shared_allocator.h>

namespace majel {
namespace malloc {
namespace detail {

void* ArenaAllocator::malloc(majel::Place place, size_t size) {
  // Forward directly to the shared allocator
  return SharedAllocator::malloc(place, size);

  // TODO: handle locally if we need more performance
}

void ArenaAllocator::free(majel::Place place, void* ptr) {
  // Forward directly to the shared allocator
  SharedAllocator::free(place, ptr);

  // TODO: handle locally if we need more performance
}

size_t ArenaAllocator::memory_used(majel::Place place) {
  // Forward directly to the shared allocator
  return SharedAllocator::memory_used(place);
}
}
}
}
