#pragma once

// Standard Library Includes
#include <cstddef>

namespace majel {
namespace malloc {
namespace cpu {

class SystemAllocator {
public:
  static void* malloc(size_t& index, size_t size);
  static void free(void* address, size_t size, size_t index);

public:
  static size_t index_count();

public:
  static void init();
  static void shutdown();

public:
  static bool uses_gpu();
};

}  // namespace cpu
}  // namespace malloc
}  // namespace majel
