#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>

#include "fzx/util.hpp"

namespace fzx {

/// Lock-free, single-producer, multiple-consumer "left-right" algorithm
template <typename T>
class LR
{
  static_assert(std::is_default_constructible_v<T>);
  static_assert(std::is_nothrow_move_assignable_v<T>);

  using Counter = uint64_t;
  using Index = int_fast8_t;

  static constexpr Index kLeft = 0;
  static constexpr Index kRight = 1;

public:
  /// Store the new value on the writer thread. Calling this from multiple threads is a data race.
  /// The new value is swapped with whatever value happened to be stored previously.
  void store(T& value) noexcept
  {
    const Index lr = mLR.load(std::memory_order_relaxed);
    ASSUME(lr == 0 || lr == 1);
    swap(mData[!lr], value);
    mLR.store(!lr, std::memory_order_release);

    const Index idx = mIndex.load(std::memory_order_relaxed);
    ASSUME(idx == 0 || idx == 1);

    wait(idx == kLeft ? mRCnt : mLCnt);
    mIndex.store(!idx, std::memory_order_release);
    wait(idx == kLeft ? mLCnt : mRCnt);
  }

  void load(T& out) noexcept(std::is_nothrow_copy_assignable_v<T>)
  {
    const Index idx = mIndex.load(std::memory_order_acquire);
    ASSUME(idx == 0 || idx == 1);
    Lock lock { idx == kLeft ? mLCnt : mRCnt };

    const Index lr = mLR.load(std::memory_order_acquire);
    ASSUME(lr == 0 || lr == 1);
    out = mData[lr];
  }

private:
  struct Lock
  {
    Lock(std::atomic<Counter>& cnt) noexcept : mCnt(&cnt)
    {
      mCnt->fetch_add(1, std::memory_order_acq_rel);
    }

    ~Lock() noexcept
    {
      mCnt->fetch_sub(1, std::memory_order_acq_rel);
    }

    Lock(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock& operator=(Lock&&) = delete;

  private:
    std::atomic<Counter>* mCnt;
  };

  void wait(std::atomic<Counter>& cnt)
  {
#if 1
    while (cnt.load(std::memory_order_acquire) != 0)
      std::this_thread::yield();
#else
    static const timespec kTs { 0, 1 };
    for (int i = 0; cnt.load(std::memory_order_acquire) != 0; ++i) {
      if (i == 8) {
        i = 0;
        nanosleep(&kTs, nullptr);
      }
    }
#endif
  }

private:
  T mData[2] {};
  alignas(fzx::kCacheLine) std::atomic<Index> mLR { kLeft };
  alignas(fzx::kCacheLine) std::atomic<Index> mIndex { kLeft };
  alignas(fzx::kCacheLine) std::atomic<Counter> mLCnt { 0 };
  alignas(fzx::kCacheLine) std::atomic<Counter> mRCnt { 0 };
};

} // namespace fzx
