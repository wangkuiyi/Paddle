#pragma once

#include <majel/malloc/detail/memory_block.h>

namespace majel {
namespace malloc {
namespace detail {

class MemoryBlockMetadata {
public:
  MemoryBlockMetadata(MemoryBlock::Type t,
                      size_t i,
                      size_t s,
                      size_t ts,
                      MemoryBlock* l,
                      MemoryBlock* r);
  MemoryBlockMetadata();

public:
  void update_guards();
  bool check_guards() const;

public:
  // TODO: compress this
  size_t guard_begin;
  MemoryBlock::Type type;
  size_t index;
  size_t size;
  size_t total_size;
  MemoryBlock* left_buddy;
  MemoryBlock* right_buddy;
  size_t guard_end;
};
}
}
}
