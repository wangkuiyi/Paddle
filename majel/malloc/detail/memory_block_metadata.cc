
#include <majel/malloc/detail/memory_block_metadata.h>

#include <functional>

namespace majel {
namespace malloc {
namespace detail {

MemoryBlockMetadata::MemoryBlockMetadata(MemoryBlock::Type t,
                                         size_t i,
                                         size_t s,
                                         size_t ts,
                                         MemoryBlock* l,
                                         MemoryBlock* r)
    : type(t),
      index(i),
      size(s),
      total_size(ts),
      left_buddy(l),
      right_buddy(r) {}

MemoryBlockMetadata::MemoryBlockMetadata()
    : type(MemoryBlock::INVALID_MEMORY),
      index(0),
      size(0),
      total_size(0),
      left_buddy(nullptr),
      right_buddy(nullptr) {}

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

static size_t hash(const MemoryBlockMetadata* metadata, size_t initial_seed) {
  size_t seed = initial_seed;

  hash_combine(seed, (size_t)metadata->type);
  hash_combine(seed, metadata->index);
  hash_combine(seed, metadata->size);
  hash_combine(seed, metadata->total_size);
  hash_combine(seed, metadata->left_buddy);
  hash_combine(seed, metadata->right_buddy);

  return seed;
}

void MemoryBlockMetadata::update_guards() {
  guard_begin = hash(this, 1);
  guard_end = hash(this, 2);
}

bool MemoryBlockMetadata::check_guards() const {
  return guard_begin == hash(this, 1) && guard_end == hash(this, 2);
}
}
}
}
