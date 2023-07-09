#include "fzx/items.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "fzx/util.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

namespace fzx {

// TODO: check for overflows. max size of the string storage is UINT32_MAX in total, same for items
// TODO: make Items::push return false on allocation failure or when max capacity was reached
// TODO: make the initial allocation have some predefined, bigger size

// "Faster, but more UB" or "Slower, but more correct" selector
// #define FZX_USE_MEMCPY

namespace {

struct Item
{
  uint32_t mOffset;
  uint32_t mLength;
};

static_assert(std::is_trivial_v<Item>); // can memcpy, don't have to call a destructor
static_assert(std::is_trivially_destructible_v<std::atomic<size_t>>);

constexpr auto kStorageAlign = fzx::kCacheLine;
static_assert(kStorageAlign >= sizeof(std::atomic<size_t>));
static_assert(kStorageAlign >= alignof(std::atomic<size_t>));
static_assert(kStorageAlign >= sizeof(Item));
static_assert(kStorageAlign >= alignof(Item));

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

  reserve<char>(mStrsPtr, mStrsSize, mStrsCap, s.size());
  reserve<Item>(mItemsPtr, mItemsSize, mItemsCap, 1);

  std::memcpy(mStrsPtr + kStorageAlign + mStrsSize, s.data(), s.size());
  new (mItemsPtr + kStorageAlign + mItemsSize * sizeof(Item)) Item {
    static_cast<uint32_t>(mStrsSize),
    static_cast<uint32_t>(s.size()),
  };

  mStrsSize += s.size();
  mItemsSize += 1;
}

template <typename T>
void Items::reserve(char*& mptr, size_t& msize, size_t& mcap, size_t n)
{
  const size_t cap = roundPow2(msize + n);
  if (cap <= mcap)
    return;

  // TODO: The size for aligned_alloc has to be rounded up to a power of two. Because the
  // reference count is stored alongside the data, this means that the actual allocated
  // storage can in reality store more items than the amount we save as capacity.
  const size_t size = roundPow2(kStorageAlign + cap * sizeof(T));
  auto* const data = static_cast<char*>(std::aligned_alloc(kStorageAlign, size));
  if (data == nullptr)
    throw std::bad_alloc {};

  new (data) std::atomic<size_t> { 1 };
  if (msize != 0) {
    auto* src = std::launder(reinterpret_cast<const T*>(mptr + kStorageAlign));
    // dst points to uninitialized storage, so std::launder probably isn't doing anything? Not
    // sure how that works here, but std::uninitialized_copy_n demands these to be the same type,
    // so std::launder it just because we can.
    auto* dst = std::launder(reinterpret_cast<T*>(data + kStorageAlign));

#ifndef FZX_USE_MEMCPY
    // Compiles down to memmove.
    std::uninitialized_copy_n(src, msize, dst);
#else
    // Technically UB in standard C++, because memcpy doesn't
    // start the object lifetime, but might be faster.
    std::memcpy(dst, src, msize * sizeof(T));
#endif
  }

  decRef(mptr);
  mptr = data;
  mcap = cap;
}

void Items::incRef(char* p) noexcept
{
  if (p == nullptr)
    return;
  auto* refCount = std::launder(reinterpret_cast<std::atomic<size_t>*>(p));
  refCount->fetch_add(1, std::memory_order_relaxed);
}

void Items::decRef(char* p) noexcept
{
  if (p == nullptr)
    return;
  auto* refCount = std::launder(reinterpret_cast<std::atomic<size_t>*>(p));
  if (refCount->fetch_sub(1, std::memory_order_acq_rel) == 1)
    std::free(p);
}

} // namespace fzx

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
