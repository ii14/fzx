// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/util.hpp"

namespace fzx {

/// Reference counted memory
class RcMem
{
  static constexpr auto kAlign = kCacheLine;

  struct alignas(kAlign) ControlBlock
  {
    std::atomic<size_t> mRef { 1 };
  };

  static_assert(std::is_trivially_destructible_v<ControlBlock>);

public:
  [[nodiscard]] static RcMem create(size_t size)
  {
    size = roundUp<kAlign>(size);

    auto* mem = static_cast<uint8_t*>(alignedAlloc(kAlign, size + sizeof(ControlBlock)));
    if (mem == nullptr)
      throw std::bad_alloc {};

    new (mem) ControlBlock {};

    RcMem r {};
    r.mPtr = mem;
    return r;
  }

  RcMem() noexcept = default;

  RcMem(const RcMem& b) noexcept : mPtr(b.mPtr) { ref(mPtr); }

  RcMem(RcMem&& b) noexcept : mPtr(std::exchange(b.mPtr, nullptr)) { }

  RcMem& operator=(const RcMem& b) noexcept
  {
    if (this == &b || mPtr == b.mPtr)
      return *this;
    unref(mPtr);
    mPtr = b.mPtr;
    ref(mPtr);
    return *this;
  }

  RcMem& operator=(RcMem&& b) noexcept
  {
    if (this == &b)
      return *this;
    if (mPtr == b.mPtr) {
      b.clear();
      return *this;
    }
    unref(mPtr);
    mPtr = std::exchange(b.mPtr, nullptr);
    return *this;
  }

  ~RcMem() noexcept { unref(mPtr); }

  void clear() noexcept
  {
    unref(mPtr);
    mPtr = nullptr;
  }

  [[nodiscard]] bool isNull() const noexcept { return mPtr == nullptr; }

  operator bool() const noexcept { return mPtr != nullptr; }

  uint8_t* data() noexcept
  {
    DEBUG_ASSERT(!isNull());
    return mPtr + sizeof(ControlBlock);
  }

  [[nodiscard]] const uint8_t* data() const noexcept
  {
    DEBUG_ASSERT(!isNull());
    return mPtr + sizeof(ControlBlock);
  }

private:
  static void ref(uint8_t* p) noexcept
  {
    if (p != nullptr) {
      ControlBlock& cb = *std::launder(reinterpret_cast<ControlBlock*>(p));
      cb.mRef.fetch_add(1, std::memory_order_relaxed);
    }
  }

  static void unref(uint8_t* p) noexcept
  {
    if (p != nullptr) {
      ControlBlock& cb = *std::launder(reinterpret_cast<ControlBlock*>(p));
      if (cb.mRef.fetch_sub(1, std::memory_order_acq_rel) == 1)
        alignedFree(p);
    }
  }

private:
  uint8_t* mPtr { nullptr };
};

} // namespace fzx
