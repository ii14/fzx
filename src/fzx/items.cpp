// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/items.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/util.hpp"

namespace fzx {

using Offset = uint64_t;

namespace {

// 38 bits for offset - 256GB addressable (can be compacted, strings are aligned)
constexpr Offset kItemOffsetMask = 0x3FFFFFFFFFULL;
// 25 bits for size - max 32MB per string
constexpr Offset kItemSizeMask = 0x1FFFFFFULL;
constexpr auto kItemSizeShift = 38;
// 1 bit reserved for ASCII/Unicode

} // namespace

Items::Items(Items&& b) noexcept
  : mStrs(std::move(b.mStrs))
  , mItems(std::move(b.mItems))
  , mStrsSize(std::exchange(b.mStrsSize, 0))
  , mItemsSize(std::exchange(b.mItemsSize, 0))
  , mStrsCap(std::exchange(b.mStrsCap, 0))
  , mItemsCap(std::exchange(b.mItemsCap, 0))
  , mMaxStrSize(std::exchange(b.mMaxStrSize, 0))
{
}

Items& Items::operator=(Items&& b) noexcept
{
  if (this == &b)
    return *this;
  mStrs = std::move(b.mStrs);
  mItems = std::move(b.mItems);
  mStrsSize = std::exchange(b.mStrsSize, 0);
  mItemsSize = std::exchange(b.mItemsSize, 0);
  mStrsCap = std::exchange(b.mStrsCap, 0);
  mItemsCap = std::exchange(b.mItemsCap, 0);
  mMaxStrSize = std::exchange(b.mMaxStrSize, 0);
  return *this;
}

void Items::clear() noexcept
{
  mStrs.clear();
  mItems.clear();
  mStrsSize = 0;
  mItemsSize = 0;
  mStrsCap = 0;
  mItemsCap = 0;
  mMaxStrSize = 0;
}

// NOTE: GCC's optimizations are extremely fragile in this function.
// Tiny changes (such as removing the AND on `size`) can result in
// GCC 9.4 making the entire application 9% slower. For what it's
// worth, if that happens it can be fixed by marking this as NOINLINE.
std::string_view Items::at(size_t n) const noexcept
{
  DEBUG_ASSERT(n < mItemsSize);
  Offset item = *std::launder(reinterpret_cast<const Offset*>(mItems.data()) + n);
  Offset offset = item & kItemOffsetMask;
  Offset size = (item >> kItemSizeShift) & kItemSizeMask;

  DEBUG_ASSERT(offset + size <= mStrsSize);
  return { reinterpret_cast<const char*>(mStrs.data()) + offset, size };
}

void Items::push(std::string_view s)
{
  if (s.empty())
    return;

  uint8_t* ptr = allocItem(s.size());
  std::memset(ptr, 0, roundUp<16>(s.size())); // TODO: memset only the bytes that are left
  std::memcpy(ptr, s.data(), s.size());

  if (s.size() > mMaxStrSize)
    mMaxStrSize = s.size();
}

uint8_t* Items::allocItem(size_t bytes)
{
  DEBUG_ASSERT(bytes > 0);

  if (bytes > kItemSizeMask)
    throw std::length_error { "item is too big" };

  // Align strings to 16 bytes. This simplifies loading strings into SIMD registers.
  // Loading unaligned memory can be slower, and aligning the pointers comes with some
  // overhead as well.
  //
  // In future unicode will be also stored here as 32-bit integers. Those need to be
  // aligned to 4 bytes as well, for the same reasons as above.
  //
  // The downside is that it increases the memory usage by 4 bytes per item on average.
  // If this is a problem, the alignment can be reduced to a smaller value, and then the
  // functions using SIMD have to be changed to use unaligned loads. Actually, I'm not
  // sure if using the unaligned load instruction is the actual problem, it might be just
  // crossing the cache line. If that's the case then *some* alignment is still better
  // than *no* alignment, as it should reduce the chance of that happening. Additionaly
  // there could be some heuristic to align larger strings, and don't bother with smaller
  // string like below 16 or 32 bytes? Or force alignment on every second item or something.
  //
  // TODO: choose alignment based on build options, ie. 4 bytes when compiled without SIMD?
  constexpr auto kAlign = 16;
  DEBUG_ASSERT(isMulOf<kAlign>(mStrsSize));

  const size_t itemsSize = mItemsSize + 1;
  if (itemsSize > kItemOffsetMask)
    throw std::length_error { "max item count reached" };
  const size_t strsSize = mStrsSize + roundUp<kAlign>(bytes);

  // Resize the string array
  if (strsSize > mStrsCap) {
    // TODO: round up with kOveralloc and the control block size taken into account?
    size_t cap = roundPow2(strsSize);
    auto mem = RcMem::create(cap + kOveralloc);

    if (mStrsSize != 0) {
      std::memcpy(mem.data(), mStrs.data(), mStrsSize);
    }

    mStrs = std::move(mem);
    mStrsCap = cap;
  }

  // Resize the item array
  if (itemsSize > mItemsCap) {
    size_t cap = mItemsCap == 0 ? 512 : mItemsCap * 2;
    auto mem = RcMem::create(cap * sizeof(Offset));

    if (mItemsSize != 0) {
      const auto* src = std::launder(reinterpret_cast<const Offset*>(mItems.data()));
      auto* dst = std::launder(reinterpret_cast<Offset*>(mem.data()));
      std::uninitialized_copy_n(src, mItemsSize, dst);
    }

    mItems = std::move(mem);
    mItemsCap = cap;
  }

  // Save item string pointer
  uint8_t* ptr = mStrs.data() + mStrsSize;
  // Append item offset to the items array
  new (mItems.data() + mItemsSize * sizeof(Offset))
      Offset { mStrsSize | (bytes << kItemSizeShift) };
  // Update current array sizes
  mStrsSize = strsSize;
  mItemsSize = itemsSize;
  return ptr;
}

} // namespace fzx
