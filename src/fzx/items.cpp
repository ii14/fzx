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

using Offset = size_t;

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

std::string_view Items::at(size_t n) const noexcept
{
  // Get item byte offset
  DEBUG_ASSERT(n < mItemsSize);
  Offset offset = *std::launder(reinterpret_cast<const Offset*>(mItems.data()) + n);

  const uint8_t* ptr = mStrs.data();

  // Get item header
  DEBUG_ASSERT(offset + sizeof(uint32_t) <= mStrsSize);
  auto size = loadAligned<uint32_t>(ptr + offset);

  DEBUG_ASSERT(offset + sizeof(uint32_t) + size <= mStrsSize);
  return { reinterpret_cast<const char*>(ptr) + offset + sizeof(uint32_t), size };
}

void Items::push(std::string_view s)
{
  if (s.empty())
    return;
  // Limit string size. The top bit in the header will
  // in future encode the string type - ASCII or UTF-32
  if (s.size() > 0x7FFFFFFFULL)
    throw std::length_error { "item is too big" };
  // Allocate space for header + string
  uint8_t* ptr = allocItem(sizeof(uint32_t) + s.size());
  // Store the item header with string size
  storeAligned(ptr, static_cast<uint32_t>(s.size()));
  // Copy the actual string
  std::memcpy(ptr + sizeof(uint32_t), s.data(), s.size());
  // Update biggest string size
  if (s.size() > mMaxStrSize)
    mMaxStrSize = s.size();
}

uint8_t* Items::allocItem(size_t bytes)
{
  DEBUG_ASSERT(bytes > 0);

  // Item alignment. Item memory contains uint32s, and reading unaligned memory can be slow.
  constexpr auto kAlign = sizeof(uint32_t);
  DEBUG_ASSERT(isMulOf<kAlign>(mStrsSize));

  const size_t itemsSize = mItemsSize + 1;
  const size_t strsSize = mStrsSize + roundUp<kAlign>(bytes);

  // Resize the string array
  if (strsSize > mStrsCap) {
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
    auto mem = RcMem::create(cap * sizeof(size_t));

    if (mItemsSize != 0) {
      const auto* src = std::launder(reinterpret_cast<const Offset*>(mItems.data()));
      auto* dst = std::launder(reinterpret_cast<Offset*>(mem.data()));
      std::uninitialized_copy_n(src, mItemsSize, dst);
    }

    mItems = std::move(mem);
    mItemsCap = cap;
  }

  // Save item base pointer
  uint8_t* ptr = mStrs.data() + mStrsSize;
  // Append item offset to the items array
  new (mItems.data() + mItemsSize * sizeof(Offset)) Offset { mStrsSize };
  // Update current sizes
  mStrsSize = strsSize;
  mItemsSize = itemsSize;
  return ptr;
}

} // namespace fzx
