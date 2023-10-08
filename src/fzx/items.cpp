// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/items.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/util.hpp"

namespace fzx {

// "Faster, but more UB" or "Slower, but more correct" selector
// #define FZX_USE_MEMCPY

namespace {

struct Item
{
  uint32_t mOffset;
  uint32_t mLength;
};

// can memcpy, don't have to call a destructor
static_assert(std::is_trivial_v<Item>);
static_assert(std::is_trivially_destructible_v<std::atomic<size_t>>);

constexpr auto kStorageAlign = fzx::kCacheLine;
static_assert(isPow2(kStorageAlign), "not a power of two");
static_assert(kStorageAlign >= sizeof(std::atomic<size_t>));
static_assert(kStorageAlign >= alignof(std::atomic<size_t>));
static_assert(kStorageAlign >= sizeof(Item));
static_assert(kStorageAlign >= alignof(Item));

void incRef(char* p) noexcept
{
  if (p == nullptr)
    return;
  auto* refCount = std::launder(reinterpret_cast<std::atomic<size_t>*>(p));
  refCount->fetch_add(1, std::memory_order_relaxed);
}

void decRef(char* p) noexcept
{
  if (p == nullptr)
    return;
  auto* refCount = std::launder(reinterpret_cast<std::atomic<size_t>*>(p));
  if (refCount->fetch_sub(1, std::memory_order_acq_rel) == 1)
    alignedFree(p);
}

template <typename T>
void resize(char*& mptr, size_t psize, size_t nsize)
{
  DEBUG_ASSERT(nsize > kStorageAlign);
  DEBUG_ASSERT(isMulOf<kStorageAlign>(nsize));
  auto* const data = static_cast<char*>(alignedAlloc(kStorageAlign, nsize));
  if (data == nullptr)
    throw std::bad_alloc {};

  new (data) std::atomic<size_t> { 1 };
  if (psize != 0) {
    auto* src = std::launder(reinterpret_cast<const T*>(mptr + kStorageAlign));
    auto* dst = std::launder(reinterpret_cast<T*>(data + kStorageAlign));
#ifndef FZX_USE_MEMCPY
    std::uninitialized_copy_n(src, psize, dst);
#else
    std::memcpy(dst, src, psize * sizeof(T));
#endif
  }

  decRef(mptr);
  mptr = data;
}

} // namespace

Items::Items(const Items& b) noexcept
  : mStrsPtr(b.mStrsPtr)
  , mStrsSize(b.mStrsSize)
  , mStrsCap(b.mStrsCap)
  , mItemsPtr(b.mItemsPtr)
  , mItemsSize(b.mItemsSize)
  , mItemsCap(b.mItemsCap)
{
  incRef(mStrsPtr);
  incRef(mItemsPtr);
}

Items::Items(Items&& b) noexcept
  : mStrsPtr(std::exchange(b.mStrsPtr, nullptr))
  , mStrsSize(std::exchange(b.mStrsSize, 0))
  , mStrsCap(std::exchange(b.mStrsCap, 0))
  , mItemsPtr(std::exchange(b.mItemsPtr, nullptr))
  , mItemsSize(std::exchange(b.mItemsSize, 0))
  , mItemsCap(std::exchange(b.mItemsCap, 0))
{
}

Items& Items::operator=(const Items& b) noexcept
{
  if (this == &b)
    return *this;

  if (mStrsPtr != b.mStrsPtr) {
    decRef(mStrsPtr);
    mStrsPtr = b.mStrsPtr;
    incRef(mStrsPtr);
  }
  mStrsSize = b.mStrsSize;
  mStrsCap = b.mStrsCap;

  if (mItemsPtr != b.mItemsPtr) {
    decRef(mItemsPtr);
    mItemsPtr = b.mItemsPtr;
    incRef(mItemsPtr);
  }
  mItemsSize = b.mItemsSize;
  mItemsCap = b.mItemsCap;

  return *this;
}

Items& Items::operator=(Items&& b) noexcept
{
  if (this == &b)
    return *this;

  decRef(mStrsPtr);
  mStrsPtr = std::exchange(b.mStrsPtr, nullptr);
  mStrsSize = std::exchange(b.mStrsSize, 0);
  mStrsCap = std::exchange(b.mStrsCap, 0);

  decRef(mItemsPtr);
  mItemsPtr = std::exchange(b.mItemsPtr, nullptr);
  mItemsSize = std::exchange(b.mItemsSize, 0);
  mItemsCap = std::exchange(b.mItemsCap, 0);

  return *this;
}

void Items::swap(Items& b) noexcept
{
  std::swap(mStrsPtr, b.mStrsPtr);
  std::swap(mStrsSize, b.mStrsSize);
  std::swap(mStrsCap, b.mStrsCap);
  std::swap(mItemsPtr, b.mItemsPtr);
  std::swap(mItemsSize, b.mItemsSize);
  std::swap(mItemsCap, b.mItemsCap);
}

void Items::clear() noexcept
{
  decRef(mStrsPtr);
  mStrsPtr = nullptr;
  mStrsSize = 0;
  mStrsCap = 0;

  decRef(mItemsPtr);
  mItemsPtr = nullptr;
  mItemsSize = 0;
  mItemsCap = 0;
}

std::string_view Items::at(size_t n) const noexcept
{
  DEBUG_ASSERT(n < mItemsSize);
  DEBUG_ASSERT(mItemsPtr != nullptr);
  auto& item = *(std::launder(reinterpret_cast<Item*>(mItemsPtr + kStorageAlign)) + n);
  DEBUG_ASSERT(mStrsPtr != nullptr);
  DEBUG_ASSERT(static_cast<size_t>(item.mOffset) + static_cast<size_t>(item.mLength) <= mStrsSize);
  return { mStrsPtr + kStorageAlign + item.mOffset, item.mLength };
}

void Items::push(std::string_view s)
{
  if (s.empty())
    return;

  constexpr auto kMaxSize = static_cast<size_t>(std::numeric_limits<uint32_t>::max());
  const size_t strsSize = mStrsSize + s.size();
  const size_t itemsSize = mItemsSize + 1;
  // Item index and string offset can no longer be represented as uint32_t.
  if (strsSize > kMaxSize || itemsSize > kMaxSize)
    throw std::length_error { "max storage size reached" };

  if (strsSize > mStrsCap) {
    // Space for the control block + overallocated bytes for reading out of bounds with SIMD
    constexpr auto kSpace = kStorageAlign + kOveralloc;
    const size_t size = mStrsCap == 0 ? 1024 : roundPow2(kSpace + mStrsCap + s.size());
    DEBUG_ASSERT(size > kSpace);
    DEBUG_ASSERT(size - kSpace > mStrsCap);
    resize<char>(mStrsPtr, mStrsSize, size);
    mStrsCap = size - kSpace;
  }

  if (itemsSize > mItemsCap) {
    // Space for the control block
    constexpr auto kSpace = kStorageAlign;
    const size_t size = mItemsCap == 0 ? 1024 : (kSpace + mItemsCap * sizeof(Item)) * 2;
    DEBUG_ASSERT(size > kSpace);
    DEBUG_ASSERT(size - kSpace > mItemsCap);
    resize<Item>(mItemsPtr, mItemsSize, size);
    mItemsCap = (size - kSpace) / sizeof(Item);
  }

  std::memcpy(mStrsPtr + kStorageAlign + mStrsSize, s.data(), s.size());

  new (mItemsPtr + kStorageAlign + mItemsSize * sizeof(Item)) Item {
    static_cast<uint32_t>(mStrsSize),
    static_cast<uint32_t>(s.size()),
  };

  mStrsSize = strsSize;
  mItemsSize = itemsSize;
}

} // namespace fzx
