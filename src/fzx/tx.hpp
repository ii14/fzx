// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <atomic>
#include <cstddef>

#include "fzx/config.hpp"

namespace fzx {

/// Single-producer, single-consumer, wait-free value transaction.
template <typename T>
struct Tx
{
  /// Write thread: Get write buffer.
  /// The returned reference is only valid up to the commit call.
  /// After that accessing the old reference is a data race.
  T& writeBuffer() noexcept
  {
    return mBuffers[mWrite];
  }

  /// Write thread: Commit written data and get a new write buffer.
  /// The new buffer can contain previously written garbage.
  void commit() noexcept
  {
    auto tick = ++mTicks[mWrite];
    mWrite = mUnused.exchange(mWrite, std::memory_order_acq_rel);
    mTicks[mWrite] = tick;
  }

  /// Read thread: Load received data.
  /// The returned reference is only valid up to the read call.
  /// After that accessing the old reference is a data race.
  const T& readBuffer() const noexcept
  {
    return mBuffers[mRead];
  }

  /// Read thread: Read data.
  /// Returns false if there is no new data.
  bool load() noexcept
  {
    auto tick = mTicks[mRead];
    mRead = mUnused.exchange(mRead, std::memory_order_acq_rel);
    if (mTicks[mRead] > tick)
      return true;
    mRead = mUnused.exchange(mRead, std::memory_order_acq_rel);
    return mTicks[mRead] > tick;
  }

  [[nodiscard]] size_t writeTick() const noexcept { return mTicks[mWrite]; }
  [[nodiscard]] size_t readTick() const noexcept { return mTicks[mRead]; }

private:
  T mBuffers[3] {};
  size_t mTicks[3] {};
  alignas(fzx::kCacheLine) uint8_t mWrite { 0 };
  alignas(fzx::kCacheLine) uint8_t mRead { 1 };
  alignas(fzx::kCacheLine) std::atomic<uint8_t> mUnused { 2 };
};

} // namespace fzx
