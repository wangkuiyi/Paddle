#pragma once

// Standard Library Includes
#include <cstddef>

namespace majel {
namespace malloc {
namespace detail {

// Forward Declarations
class MetadataCache;

/*! \brief A class used to interpret the contents of a memory block allocated by
 * majel. */
class MemoryBlock {
public:
  enum Type { FREE_MEMORY, ARENA_CHUNK, HUGE_CHUNK, INVALID_MEMORY };

public:
  void initialize(MetadataCache& cache,
                  Type t,
                  size_t index,
                  size_t size,
                  void* left_buddy,
                  void* right_buddy);

public:
  /*! \brief The type of allocation */
  Type type(MetadataCache& cache) const;
  /*! \brief The size of the data region */
  size_t size(MetadataCache& cache) const;
  /*! \brief The total size of the block */
  size_t total_size(MetadataCache& cache) const;
  /*! \brief An index used to track the allocator that produced this allocation
   */
  size_t index(MetadataCache& cache) const;

public:
  /*! \brief Get the left buddy */
  MemoryBlock* left_buddy(MetadataCache& cache) const;
  /*! \brief Get the right buddy */
  MemoryBlock* right_buddy(MetadataCache& cache) const;

public:
  /*! \brief Split the allocation into left/right blocks */
  void split(MetadataCache& cache, size_t size);

  /*! \brief Merge left and right blocks together */
  void merge(MetadataCache& cache, MemoryBlock* right_buddy);

  /*! \brief Mark the allocation as free */
  void mark_as_free(MetadataCache& cache);

  /*! \brief Change the type of the allocaiton */
  void set_type(MetadataCache& cache, Type t);

public:
  bool has_left_buddy(MetadataCache& cache) const;
  bool has_right_buddy(MetadataCache& cache) const;

public:
  /*! \brief Get a pointer to the memory block's data */
  void* data() const;
  /*! \brief Get a pointer to the memory block's metadata */
  MemoryBlock* metadata() const;

public:
  static size_t overhead();
};
}
}
}
