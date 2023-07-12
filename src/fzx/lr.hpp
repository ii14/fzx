#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>

#include "fzx/config.hpp"

namespace fzx {

/// Single-producer, multiple-consumer "left-right" algorithm.
///
/// Left-Right: A Concurrency Control Technique with Wait-Free Population Oblivious Reads
/// Pedro Ramalhete, Anderia Correia
/// https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/left-right-2014.pdf
template <typename T>
class LR
{
  static_assert(std::is_default_constructible_v<T>);

  using Counter = uint64_t;
  using Index = int_fast8_t;

public:
  /// Store the new value on the writer thread.
  /// Calling this from multiple threads is a data race.
  void store(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
  {
    // Only modified by this thread, so a relaxed load is okay.
    const Index idx = mDataIdx.load(std::memory_order_relaxed);

    // Swap the data and point new readers at it.
    mData[!idx] = value;
    mDataIdx.store(!idx, std::memory_order_seq_cst);

    auto wait = [](std::atomic<Counter>& counter) {
#if 1
      while (counter.load(std::memory_order_seq_cst) != 0)
        std::this_thread::yield();
#else
      static const timespec kTs { 0, 1 };
      for (int i = 0; counter.load(std::memory_order_seq_cst) != 0; ++i) {
        if (i == 8) {
          i = 0;
          nanosleep(&kTs, nullptr);
        }
      }
#endif
    };

    // Reusing idx for the reference count index below can be misleading,
    // because readers sitting on either reference counter can be reading
    // either side of the data.

    // No new readers have access to this reference counter, but some of
    // the existing ones might be still using it. We have to wait before
    // it can be exposed it again.
    wait(readerCount(!idx));
    // Redirect new readers to use the new reference counter.
    mCountIdx.store(!idx, std::memory_order_seq_cst);
    // Now also wait for any remaining readers on the previous reference
    // counter, to make sure no one has the access to the previous data,
    // and everyone has moved to the just stored data index.
    wait(readerCount(idx));
  }

  /// Load data. Thread-safe, wait-free.
  void load(T& out) noexcept(std::is_nothrow_copy_assignable_v<T>)
  {
    // Get the current reference counter index and tell the world that
    // we're now reading it.
    const Index cidx = mCountIdx.load(std::memory_order_seq_cst);
    Lock lock { readerCount(cidx) };

    const Index didx = mDataIdx.load(std::memory_order_seq_cst);
    out = mData[didx];
  }

private:
  struct Lock
  {
    Lock(std::atomic<Counter>& cnt) noexcept : mReaderCount(&cnt)
    {
      mReaderCount->fetch_add(1, std::memory_order_seq_cst);
    }

    ~Lock() noexcept
    {
      mReaderCount->fetch_sub(1, std::memory_order_release);
    }

    Lock(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock& operator=(Lock&&) = delete;

  private:
    std::atomic<Counter>* mReaderCount;
  };

  [[nodiscard]] std::atomic<Counter>& readerCount(Index idx)
  {
    return idx ? mRCount1 : mRCount2;
  }

private:
  T mData[2] { {}, {} };
  alignas(fzx::kCacheLine) std::atomic<Index> mDataIdx { 0 };
  alignas(fzx::kCacheLine) std::atomic<Index> mCountIdx { 0 };
  alignas(fzx::kCacheLine) std::atomic<Counter> mRCount1 { 0 };
  alignas(fzx::kCacheLine) std::atomic<Counter> mRCount2 { 0 };
};

} // namespace fzx
