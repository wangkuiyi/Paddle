
#pragma once

// Standard Library Includes
#include <cstddef>

namespace majel {
namespace malloc {
namespace detail {

/*! Allocator configuration parameters */
class Parameters {
public:
  /*! \brief The size that all allocations from the last level allocator are
   * aligned to. */
  static size_t arena_chunk_size();
  /*! \brief The size that all allocations from the gpu system allocator are
   * aligned to */
  static size_t gpu_system_chunk_size();
  /*! \brief The maximum size that can be allocated on the gpu directly. */
  static size_t gpu_maximum_allocation_size();
  /*! \brief The maximum size that can be allocated. */
  static size_t system_maximum_allocation_size();
  /*! \brief The size that all allocations from the cpu system allocator are
   * aligned to */
  static size_t cpu_system_chunk_size();
  /*! \brief The fraction of GPU memory to use for majel allocations. */
  static double fraction_of_gpu_memory_to_use();
  /*! \brief Should all allocations be initialized with nans */
  static bool should_initialize_allocations();
  /*! \brief Is allocating GPU memory allowed? */
  static bool is_allocating_gpu_memory_allowed();

public:
  /*! \brief Set whether or not allocating gpu memory is allowed. */
  static void set_allow_gpu_memory_use(bool allow);
};
}
}
}
