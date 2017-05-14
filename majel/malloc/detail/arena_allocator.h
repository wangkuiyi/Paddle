
#pragma once

#include <majel/place.h>

namespace majel {
namespace malloc {
namespace detail {

/*! \brief The private memory allocator for a single host thread. */
class ArenaAllocator {
public:
  static void* malloc(majel::Place place, size_t size);
  static void free(majel::Place place, void* ptr);
  static size_t memory_used(majel::Place place);
};
}
}
}
