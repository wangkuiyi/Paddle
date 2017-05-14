
#include <majel/malloc/detail/parameters.h>

#include <majel/gpu/detail/cuda.h>

#include <majel/contrib/support/cpu_info.h>

#include <majel/contrib/logger/logger.h>
#include <majel/contrib/support/knobs.h>

namespace majel {
namespace malloc {
namespace detail {

size_t Parameters::arena_chunk_size() {
  // 16KB chunks
  return support::get_knob_value("MAJEL_ARENA_MALLOC_CHUNK_SIZE", (1 << 8));
}

size_t Parameters::gpu_system_chunk_size() {
  // fraction_of_gpu_memory_usage% of available GPU memory
  size_t total = 0;
  size_t available = 0;

  // determine usable memory size
  gpu::detail::get_memory_usage(available, total);

  // size down to be sure that the allocation will succeed
  size_t buffer = (1.0 - fraction_of_gpu_memory_to_use()) * total;

  available = std::max(available, arena_chunk_size()) - arena_chunk_size();
  size_t usable = std::max(available, buffer) - buffer;

  size_t size =
      support::get_knob_value("MAJEL_GPU_SYSTEM_MALLOC_CHUNK_SIZE", usable);

  logger::log("MallocParameters") << "GPU system chunk size is " << size << " ("
                                  << available << " available, " << total
                                  << " total)\n";

  return size;
}

size_t Parameters::gpu_maximum_allocation_size() {
  // fraction_of_gpu_memory_usage% of GPU memory
  size_t total = 0;
  size_t available = 0;

  gpu::detail::get_memory_usage(available, total);

  return support::get_knob_value("MAJEL_GPU_SYSTEM_MALLOC_CHUNK_SIZE",
                                 total * fraction_of_gpu_memory_to_use());
}

size_t Parameters::system_maximum_allocation_size() {
  return support::get_knob_value(
             "MAJEL_GPU_SYSTEM_MALLOC_OVERSUBSCRIPTION_FACTOR", 2) *
         gpu_maximum_allocation_size();
}

size_t Parameters::cpu_system_chunk_size() {
  // 3% of CPU memory
  return support::get_knob_value("MAJEL_CPU_SYSTEM_MALLOC_CHUNK_SIZE",
                                 support::get_total_physical_memory() / 32);
}

double Parameters::fraction_of_gpu_memory_to_use() {
  // Use 95% of GPU memory for MAJEL arrays, reserve the rest for page tables,
  // etc
  return support::get_knob_value("MAJEL_FRACTION_OF_GPU_MEMORY_TO_USE", 0.95);
}

bool Parameters::should_initialize_allocations() {
  return support::get_knob_value("MAJEL_INITIALIZE_ALLOCATIONS", false);
}

static bool allow_gpu_memory_use_ = true;

bool Parameters::is_allocating_gpu_memory_allowed() {
  return allow_gpu_memory_use_;
}

void Parameters::set_allow_gpu_memory_use(bool allow) {
  allow_gpu_memory_use_ = allow;
}
}
}
}
