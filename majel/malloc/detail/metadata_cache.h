
#pragma once

#include <majel/malloc/detail/memory_block.h>
#include <majel/malloc/detail/memory_block_metadata.h>

#include <unordered_map>

namespace majel {
namespace malloc {
namespace detail {

/*! A cache for accessing memory block meta-data that may be expensive to access
   directly.

    Note: this class exists to unify the metadata format between GPU and CPU
   allocations.
    It should be removed when the CPU can access all GPU allocations directly
   via UVM.
*/
class MetadataCache {
public:
  MetadataCache(bool uses_gpu);
  ~MetadataCache();

public:
  /*! \brief Load the associated metadata for the specified memory block. */
  MemoryBlockMetadata load(const MemoryBlock*);

  /*! \brief Store the associated metadata for the specified memory block. */
  void store(MemoryBlock*, const MemoryBlockMetadata&);

public:
  /*! \brief Acquire any external metadata updates. */
  void acquire(MemoryBlock*);

  /*! \brief Publish any local updates externally. */
  void release(MemoryBlock*);

  /*! \brief Indicate that the specified metadata will no longer be used */
  void invalidate(MemoryBlock*);

public:
  MetadataCache(const MetadataCache&) = delete;
  MetadataCache& operator=(const MetadataCache&) = delete;

private:
  bool uses_gpu_;

private:
  typedef std::unordered_map<const MemoryBlock*, MemoryBlockMetadata>
      MetadataMap;

private:
  MetadataMap cache_;
};
}
}
}
