

// Majel Includes
#include <majel/malloc/detail/utility.h>

namespace majel {
namespace malloc {
namespace detail {

size_t align(size_t size, size_t alignment) {
  size_t remainder = size % alignment;

  return remainder == 0 ? size : size + alignment - remainder;
}
}
}
}
