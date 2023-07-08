#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fzx {

/// Push-only item vector.
///
/// The internal storage is shared (and reference counted) between the copies. This assumes that
/// only the most up to date instance pushes the items. Otherwise strings can get overwritten which
/// also potentially makes it a data race, because read-only copies should be safely accessed from
/// different threads. TODO: Could be made safer to use by splitting this into two classes.
struct Items
{
  Items() noexcept = default;
  Items(const Items& b) noexcept;
  Items(Items&& b) noexcept;
  Items& operator=(const Items& b) noexcept;
  Items& operator=(Items&& b) noexcept;
  ~Items() noexcept { clear(); }
  void swap(Items& b) noexcept;
  void clear() noexcept;

  [[nodiscard]] size_t size() const noexcept { return mItemsSize; }
  [[nodiscard]] std::string_view at(size_t n) const noexcept;

  /// Push a new string into the vector.
  /// NOTE: Only the most up-to-date instance can do this.
  void push(std::string_view s);

private:
  template <typename T>
  void reserve(char*& mptr, size_t& msize, size_t& mcap, size_t n);
  static void incRef(char* p) noexcept;
  static void decRef(char* p) noexcept;

private:
  char* mStrsPtr { nullptr };
  size_t mStrsSize { 0 };
  size_t mStrsCap { 0 };
  char* mItemsPtr { nullptr };
  size_t mItemsSize { 0 };
  size_t mItemsCap { 0 };
};

inline void swap(Items& a, Items& b) noexcept
{
  a.swap(b);
}

} // namespace fzx
