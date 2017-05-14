
#pragma once

#include <majel/place.h>

namespace majel {
namespace malloc {
namespace detail {

/*! \brief The shared last level majel allocator. */
class SharedAllocator {
public:
  static void* malloc(majel::Place place, size_t size);
  static void free(majel::Place place, void* ptr);
  static size_t memory_used(majel::Place place);

public:
  static void init();
  static void shutdown();
};
}
}
}
