
#include <majel/malloc/detail/memory_block_metadata.h>
#include <majel/malloc/detail/metadata_cache.h>

#include <majel/gpu/detail/cuda.h>

namespace majel {
namespace malloc {
namespace detail {

MetadataCache::MetadataCache(bool uses_gpu) : uses_gpu_(uses_gpu) {}

MetadataCache::~MetadataCache() {}

MemoryBlockMetadata MetadataCache::load(const MemoryBlock* block) {
  if (uses_gpu_) {
    auto existing_metadata = cache_.find(block);

    // fill on miss
    if (existing_metadata == cache_.end()) {
      cudaStream_t stream = majel::gpu::detail::create_new_stream();

      MemoryBlockMetadata metadata;

      majel::gpu::detail::memcpy(&metadata,
                                 block,
                                 sizeof(MemoryBlockMetadata),
                                 cudaMemcpyDeviceToHost,
                                 stream);
      majel::gpu::detail::wait_for_stream(stream);

      majel::gpu::detail::destroy_stream(stream);

      existing_metadata = cache_.insert(std::make_pair(block, metadata)).first;
    }

    assert(existing_metadata->second.check_guards());

    return existing_metadata->second;
  } else {
    assert(reinterpret_cast<const MemoryBlockMetadata*>(block)->check_guards());

    return *reinterpret_cast<const MemoryBlockMetadata*>(block);
  }
}

void MetadataCache::store(MemoryBlock* block,
                          const MemoryBlockMetadata& original_metadata) {
  auto metadata = original_metadata;

  metadata.update_guards();

  if (uses_gpu_) {
    cache_[block] = metadata;
  } else {
    *reinterpret_cast<MemoryBlockMetadata*>(block) = metadata;
  }
}

void MetadataCache::acquire(MemoryBlock*) {
  assert(false && "Not Implemented");
}

void MetadataCache::release(MemoryBlock*) {
  assert(false && "Not Implemented");
}

void MetadataCache::invalidate(MemoryBlock* block) {
  if (uses_gpu_) {
    auto existing_metadata = cache_.find(block);

    assert(existing_metadata != cache_.end());

    cache_.erase(existing_metadata);
  }
}
}
}
}
