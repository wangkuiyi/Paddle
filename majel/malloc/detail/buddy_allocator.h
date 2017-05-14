
#pragma once

// Majel Includes
#include <majel/malloc/detail/memory_block.h>
#include <majel/malloc/detail/metadata_cache.h>
#include <majel/malloc/detail/parameters.h>
#include <majel/malloc/detail/utility.h>

#include <majel/gpu/detail/cuda.h>

#include <majel/contrib/logger/logger.h>

// Standard Library Includes
#include <mutex>
#include <set>

namespace majel {
namespace malloc {
namespace detail {

template <typename BaseAllocator>
class BuddyAllocator {
public:
  BuddyAllocator(size_t minimum_allocation_size, size_t maximum_allocation_size)
      : minimum_allocation_size_(minimum_allocation_size),
        maximum_allocation_size_(maximum_allocation_size),
        should_initialize_(Parameters::should_initialize_allocations()),
        total_used_(0),
        total_free_(0),
        fallback_allocations_(0),
        cache_(BaseAllocator::uses_gpu()) {}

  ~BuddyAllocator() {
    while (!allocations_.empty()) {
      // TODO: make sure that all of these have actually been freed

      auto block =
          static_cast<MemoryBlock*>(std::get<2>(*allocations_.begin()));
      BaseAllocator::free(
          block, maximum_allocation_size_, block->index(cache_));
      cache_.invalidate(block);
      allocations_.erase(allocations_.begin());
    }
  }

public:
  void* malloc(size_t unaligned_size) {
    size_t size = align(unaligned_size + MemoryBlock::overhead(),
                        std::max(sizeof(size_t), minimum_allocation_size_));

    // acquire the allocator lock
    std::unique_lock<std::mutex> lock(mutex_);

    logger::log("BuddyAllocator") << "Malloc " << unaligned_size
                                  << " bytes from chunk size " << size << "\n";

    // if the allocation is huge, send directly to the base allocator
    if (size > maximum_allocation_size_) {
      logger::log("BuddyAllocator") << " allocating from base allocator\n";
      void* base_allocation = allocate_from_base_(size);

      if (base_allocation == nullptr) {
        return nullptr;
      }

      return initialize_(base_allocation, unaligned_size);
    }

    // try to allocate from an existing chunk
    auto existing_allocation = find_best_existing_allocation_with_space_(size);

    // refill the allocation pool on failure
    if (existing_allocation == allocations_.end()) {
      existing_allocation = refill_allocations_();
    } else {
      logger::log("BuddyAllocator")
          << " allocating from existing memory block "
          << std::get<2>(*existing_allocation) << " at address "
          << reinterpret_cast<MemoryBlock*>(std::get<2>(*existing_allocation))
                 ->data()
          << "\n";
    }

    // if we fail after refill, fail fatally
    if (existing_allocation == allocations_.end()) {
      return nullptr;
    }

    total_free_ -= size;
    total_used_ += size;

    // split the allocation and prepare it for use
    void* address = split_and_prepare_allocation_(existing_allocation, size);

    return initialize_(reinterpret_cast<MemoryBlock*>(address)->data(),
                       unaligned_size);
  }

  void free(void* address) {
    auto block = reinterpret_cast<MemoryBlock*>(address)->metadata();

    // acquire the allocator lock
    std::unique_lock<std::mutex> lock(mutex_);

    logger::log("BuddyAllocator") << "Free from address " << block << "\n";

    // Ask the base allocator to free huge chunks
    if (block->type(cache_) == MemoryBlock::HUGE_CHUNK) {
      logger::log("BuddyAllocator")
          << " Freeing directly from base allocator\n";

      BaseAllocator::free(
          block, block->total_size(cache_), block->index(cache_));

      cache_.invalidate(block);
      return;
    }

    block->mark_as_free(cache_);

    total_used_ -= block->total_size(cache_);
    total_free_ += block->total_size(cache_);

    // Try to merge with the right buddy
    if (block->has_right_buddy(cache_)) {
      logger::log("BuddyAllocator") << " merging this block " << block
                                    << " with right buddy "
                                    << block->right_buddy(cache_) << "\n";

      block = merge_with_right_buddy_(block, block->right_buddy(cache_));
    }

    // Try to merge with the left buddy
    if (block->has_left_buddy(cache_)) {
      logger::log("BuddyAllocator") << " merging this block " << block
                                    << " with left buddy "
                                    << block->left_buddy(cache_) << "\n";

      block = merge_with_left_buddy_(block, block->left_buddy(cache_));
    }

    // Add the block to the allocation set if the buddy is still in use
    logger::log("BuddyAllocator") << " inserting free block (" << block << ", "
                                  << block->total_size(cache_) << ")\n";
    allocations_.insert(IndexSizeAndAddress(
        block->index(cache_), block->total_size(cache_), block));

    initialize_(block->data(), block->size(cache_));

    // Clean up if there is too much free memory
    clean_allocations_();
  }

  size_t memory_used() const { return total_used_; }

  bool empty() const { return (total_free_ + total_used_) == 0; }

private:
  typedef std::tuple<size_t, size_t, void*> IndexSizeAndAddress;
  typedef std::set<IndexSizeAndAddress> AllocationSet;

private:
  void* allocate_from_base_(size_t size) {
    size_t index = 0;

    void* new_allocation = BaseAllocator::malloc(index, size);

    logger::log("BuddyAllocator") << " allocated " << new_allocation
                                  << " from base allocator\n";

    // check for failure
    if (new_allocation == nullptr) {
      return nullptr;
    }

    // Construct a memory block
    static_cast<MemoryBlock*>(new_allocation)
        ->initialize(
            cache_, MemoryBlock::HUGE_CHUNK, index, size, nullptr, nullptr);

    // Check for fallback block
    if (is_fallback_allocation_(static_cast<MemoryBlock*>(new_allocation))) {
      logger::log("BuddyAllocator::Detail")
          << "Allocated fallback allocation with "
          << (100. * total_free_ / total_used_) << "% fragmentation.\n";
    }

    return static_cast<MemoryBlock*>(new_allocation)->data();
  }

  AllocationSet::iterator refill_allocations_() {
    // Reset the maximum allocation size for new GPU allocations
    if (BaseAllocator::uses_gpu()) {
      if (empty()) {
        maximum_allocation_size_ = Parameters::gpu_system_chunk_size();
      }
    }

    // Allocate a new maximum sized block
    size_t index = 0;

    void* new_allocation =
        BaseAllocator::malloc(index, maximum_allocation_size_);

    if (new_allocation == nullptr) {
      return allocations_.end();
    }

    logger::log("BuddyAllocator") << " creating and inserting new block "
                                  << new_allocation << " from base allocator\n";

    static_cast<MemoryBlock*>(new_allocation)
        ->initialize(cache_,
                     MemoryBlock::FREE_MEMORY,
                     index,
                     maximum_allocation_size_,
                     nullptr,
                     nullptr);

    // Check for fallback block
    if (is_fallback_allocation_(static_cast<MemoryBlock*>(new_allocation))) {
      logger::log("BuddyAllocator::Detail")
          << "Allocated fallback allocation with "
          << (100. * total_free_ / total_used_) << "% fragmentation.\n";
      ++fallback_allocations_;
    }

    total_free_ += maximum_allocation_size_;

    // Insert it into the list of allocations
    return allocations_
        .insert(IndexSizeAndAddress(
            index, maximum_allocation_size_, new_allocation))
        .first;
  }

  void* split_and_prepare_allocation_(AllocationSet::iterator allocation,
                                      size_t size) {
    auto block = static_cast<MemoryBlock*>(std::get<2>(*allocation));

    allocations_.erase(allocation);

    logger::log("BuddyAllocator") << "  spliting block (" << block << ", "
                                  << block->total_size(cache_) << ") into\n";

    block->split(cache_, size);

    logger::log("BuddyAllocator") << "   left block (" << block << ", "
                                  << block->total_size(cache_) << ")\n";

    block->set_type(cache_, MemoryBlock::ARENA_CHUNK);

    if (block->has_right_buddy(cache_)) {
      if (block->right_buddy(cache_)->type(cache_) ==
          MemoryBlock::FREE_MEMORY) {
        logger::log("BuddyAllocator")
            << "   inserting right block (" << block->right_buddy(cache_)
            << ", " << block->right_buddy(cache_)->total_size(cache_) << ")\n";

        allocations_.insert(
            IndexSizeAndAddress(block->right_buddy(cache_)->index(cache_),
                                block->right_buddy(cache_)->total_size(cache_),
                                block->right_buddy(cache_)));
      }
    }

    if (is_fallback_allocation_(block)) {
      logger::log("GpuDefaultAllocator")
          << "Allocation of size " << size
          << " is reusing an allocation returned by a fallback GPU "
             "allocator.\n";
    }

    return block;
  }

  MemoryBlock* merge_with_right_buddy_(MemoryBlock* block,
                                       MemoryBlock* right_buddy) {
    // fail if the buddy is not free
    if (right_buddy->type(cache_) != MemoryBlock::FREE_MEMORY) {
      logger::log("BuddyAllocator") << "  failed for block " << block
                                    << ", right buddy " << right_buddy
                                    << " is not free\n";
      return block;
    }

    // remove the buddy from the list of free allocations
    auto right_buddy_allocation =
        allocations_.find(IndexSizeAndAddress(right_buddy->index(cache_),
                                              right_buddy->total_size(cache_),
                                              right_buddy));

    assert(right_buddy_allocation != allocations_.end());

    allocations_.erase(right_buddy_allocation);

    // merge
    block->merge(cache_, right_buddy);

    return block;
  }

  MemoryBlock* merge_with_left_buddy_(MemoryBlock* block,
                                      MemoryBlock* left_buddy) {
    // fail if the buddy is not free
    if (left_buddy->type(cache_) != MemoryBlock::FREE_MEMORY) {
      logger::log("BuddyAllocator") << "  failed for block " << block
                                    << ", left buddy " << left_buddy
                                    << " is not free\n";
      return block;
    }

    // remove the buddy from the list of free allocations
    auto left_buddy_allocation = allocations_.find(IndexSizeAndAddress(
        left_buddy->index(cache_), left_buddy->total_size(cache_), left_buddy));

    assert(left_buddy_allocation != allocations_.end());

    allocations_.erase(left_buddy_allocation);

    // merge
    left_buddy->merge(cache_, block);

    return left_buddy;
  }

  void clean_allocations_() {
    // prefer freeing fallback allocations
    free_fallback_allocations_();

    free_normal_allocations_();
  }

  void free_fallback_allocations_() {
    if (fallback_allocations_ == 0) {
      return;
    }

    for (auto allocation = allocations_.rbegin();
         allocation != allocations_.rend();) {
      if (std::get<1>(*allocation) < maximum_allocation_size_) {
        return;
      }

      MemoryBlock* block = static_cast<MemoryBlock*>(std::get<2>(*allocation));

      if (!is_fallback_allocation_(block)) {
        return;
      }

      logger::log("BuddyAllocator") << " returning block " << block
                                    << " to fallback allocator\n";

      // free from the system allocator
      BaseAllocator::free(
          block, maximum_allocation_size_, block->index(cache_));
      cache_.invalidate(block);

      allocation = AllocationSet::reverse_iterator(
          allocations_.erase(std::next(allocation).base()));

      total_free_ -= maximum_allocation_size_;
      --fallback_allocations_;

      if (fallback_allocations_ == 0) {
        return;
      }
    }
  }

  void free_normal_allocations_() {
    // keep 2x overhead
    if (!should_free_allocations_()) {
      return;
    }

    // return memory to the system allocators until the ratio is maintained
    auto rbegin = AllocationSet::reverse_iterator(
        allocations_.lower_bound(std::make_tuple(1, 0, nullptr)));

    for (auto allocation = rbegin; allocation != allocations_.rend();) {
      if (std::get<1>(*allocation) < maximum_allocation_size_) {
        return;
      }

      MemoryBlock* block = static_cast<MemoryBlock*>(std::get<2>(*allocation));

      logger::log("BuddyAllocator") << " returning block " << block
                                    << " to base allocator\n";

      // free from the system allocator
      BaseAllocator::free(
          block, maximum_allocation_size_, block->index(cache_));
      cache_.invalidate(block);

      allocation = AllocationSet::reverse_iterator(
          allocations_.erase(std::next(allocation).base()));

      total_free_ -= maximum_allocation_size_;

      if (!should_free_allocations_()) {
        return;
      }
    }
  }

  bool should_free_allocations_() const {
    // free all fallback allocations
    if (fallback_allocations_ > 0) {
      return true;
    }

    // keep 2x overhead if we haven't fallen back
    if ((total_used_ + maximum_allocation_size_) * 2 < total_free_) {
      return true;
    }

    return false;
  }

  void* initialize_(void* data, size_t size) {
    if (!should_initialize_) {
      return data;
    }

    int initial_value = 0xff;

    if (BaseAllocator::uses_gpu()) {
      majel::gpu::detail::memset_sync(data, initial_value, size);
    } else {
      std::memset(data, initial_value, size);
    }

    return data;
  }

  AllocationSet::iterator find_best_existing_allocation_with_space_(
      size_t size) {
    size_t next_index = 0;

    // iterate through indices, find the best match
    while (true) {
      auto allocation = allocations_.lower_bound(
          IndexSizeAndAddress(next_index, size, nullptr));

      if (allocation == allocations_.end()) {
        return allocation;
      }

      if (std::get<0>(*allocation) > next_index) {
        if (std::get<1>(*allocation) >= size) {
          return allocation;
        }

        next_index = std::get<0>(*allocation);
        continue;
      }

      return allocation;
    }
  }

  bool is_fallback_allocation_(MemoryBlock* block) {
    if (!BaseAllocator::uses_gpu()) {
      return false;
    }

    return block->index(cache_) > 0;
  }

public:
  BuddyAllocator(const BuddyAllocator&) = delete;
  BuddyAllocator& operator=(const BuddyAllocator&) = delete;

private:
  size_t minimum_allocation_size_;
  size_t maximum_allocation_size_;
  bool should_initialize_;

private:
  size_t total_used_;
  size_t total_free_;
  size_t fallback_allocations_;

private:
  AllocationSet allocations_;

private:
  MetadataCache cache_;

private:
  std::mutex mutex_;
};
}
}
}
