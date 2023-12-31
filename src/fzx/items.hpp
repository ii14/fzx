// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cstddef>
#include <string_view>

#include "fzx/rc_mem.hpp"

namespace fzx {

/// Push-only item vector.
///
/// The internal storage is shared (and reference counted) between the copies. This assumes that
/// only the most up to date instance pushes the items. Otherwise strings can get overwritten which
/// also potentially makes it a data race, because read-only copies should be safely accessed from
/// different threads.
struct Items
{
  Items() noexcept = default;
  Items(const Items&) noexcept = default;
  Items& operator=(const Items&) noexcept = default;
  Items(Items&&) noexcept;
  Items& operator=(Items&&) noexcept;
  ~Items() noexcept = default;

  void clear() noexcept;

  [[nodiscard]] size_t size() const noexcept { return mItemsSize; }

  /// Get the item at given index.
  ///
  /// NOTE: Accessing an item out of range is undefined behavior.
  [[nodiscard]] std::string_view at(size_t n) const noexcept;

  /// Push a new string into the vector.
  ///
  /// NOTE: Internal storage is shared between the copies. Only the most up-to-date
  /// copy can call this method. Otherwise it's undefined behavior (data race).
  void push(std::string_view s);

  [[nodiscard]] size_t maxStrSize() const noexcept { return mMaxStrSize; }

private:
  /// Allocate space for new item, append it to the items array and return the item base pointer
  uint8_t* allocItem(size_t bytes);

private:
  RcMem mStrs; ///< Item storage
  RcMem mItems; ///< Item offsets, for random access
  size_t mStrsSize { 0 };
  size_t mItemsSize { 0 };
  size_t mStrsCap { 0 };
  size_t mItemsCap { 0 };
  size_t mMaxStrSize { 0 };
};

} // namespace fzx
