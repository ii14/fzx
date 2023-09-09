// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

namespace fzx {

/// Take N items at once from the queue. This value can be adjusted.
/// Higher values might result in the work being split unevenly across the threads.
/// Lower values will synchronize threads more often and will have more L1 cache misses.
static constexpr auto kChunkSize = 0x4000;

/// Hard limit on 64 threads.
static constexpr unsigned kMaxThreads = 64;

/// CPU cache line size.
static constexpr auto kCacheLine = 64;

/// Bytes to overallocate.
/// Ensures that SIMD instructions can read unaligned
/// out of bounds memory, without triggering a segfault.
static constexpr auto kOveralloc = 64;

} // namespace fzx
