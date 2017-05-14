#pragma once
#include <majel/place.h>

namespace majel {
namespace malloc {

void init();
void shutdown();

void* malloc(majel::Place place, size_t size);
void free(majel::Place place, void* ptr);
size_t memory_used(majel::Place);
}
}
