// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <utility>

#include "fzx/config.hpp"

namespace fzx {

/// Atomic counter for adding queue functionality on top of fzx::Items
struct ItemQueue
{
  // This atomic counter is not synchronizing anything, hence the relaxed atomics.

  /// Reserve `n` items and return the start index of reserved range.
  [[nodiscard]] size_t take(size_t n) noexcept
  {
    return mIndex.fetch_add(n, std::memory_order_relaxed);
  }

  /// Reserve `n` items, up to `max` items, and return the reserved range.
  /// unused atm
  [[nodiscard]] std::pair<size_t, size_t> take(size_t n, size_t max) noexcept
  {
    size_t expected = mIndex.load(std::memory_order_relaxed);
    size_t desired; // NOLINT(cppcoreguidelines-init-variables)
    do {
      desired = std::min(expected + n, max);
      if (desired == max)
        return { 0, 0 };
    } while (!mIndex.compare_exchange_weak(expected, expected + n, std::memory_order_relaxed,
                                           std::memory_order_relaxed));
    return { expected, desired };
  }

  /// Get the current value, for reporting the progress.
  [[nodiscard]] size_t get() const noexcept { return mIndex.load(std::memory_order_relaxed); }

private:
  alignas(kCacheLine) std::atomic<size_t> mIndex { 0 };
};

} // namespace fzx
