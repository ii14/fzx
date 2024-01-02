#pragma once

#include <memory>
#include <new>
#include <string_view>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/util.hpp"

namespace fzx {

/// Read-only string that is guaranteed to be aligned to the cache line size.
/// The underlying memory is overallocated to a multiple of the cache line size.
struct AlignedString
{
  constexpr AlignedString() noexcept = default;

  AlignedString(std::string_view str)
  {
    if (str.empty())
      return;
    size_t size = roundUp<kCacheLine>(str.size());
    mPtr = static_cast<char*>(alignedAlloc(kCacheLine, size));
    if (mPtr == nullptr)
      throw std::bad_alloc {};
    mEnd = std::uninitialized_copy_n(str.data(), str.size(), mPtr);
    std::uninitialized_fill_n(mEnd, size - str.size(), 0); // zero out the rest of the memory
  }

  AlignedString(AlignedString&& b) noexcept
    : mPtr(std::exchange(b.mPtr, nullptr)), mEnd(std::exchange(b.mEnd, nullptr))
  {
  }

  AlignedString& operator=(AlignedString&& b) noexcept
  {
    if (this == &b)
      return *this;
    mPtr = std::exchange(b.mPtr, nullptr);
    mEnd = std::exchange(b.mEnd, nullptr);
    return *this;
  }

  ~AlignedString() noexcept { alignedFree(mPtr); }

  void clear() noexcept
  {
    alignedFree(mPtr);
    mPtr = nullptr;
    mEnd = nullptr;
  }

  // not going to bother implementing copy, won't be used
  AlignedString(const AlignedString&) = delete;
  AlignedString& operator=(const AlignedString&) = delete;

  [[nodiscard]] const char* data() const noexcept { return mPtr; }
  [[nodiscard]] size_t size() const noexcept { return mEnd - mPtr; }
  [[nodiscard]] bool empty() const noexcept { return mPtr == mEnd; }

  [[nodiscard]] std::string_view str() const noexcept { return { mPtr, size() }; }
  operator std::string_view() const noexcept { return { mPtr, size() }; }

  [[nodiscard]] const char* begin() const noexcept { return mPtr; }
  [[nodiscard]] const char* end() const noexcept { return mEnd; }

  [[nodiscard]] char operator[](ptrdiff_t i) const noexcept
  {
    DEBUG_ASSERT(i >= 0);
    DEBUG_ASSERT(mPtr != nullptr);
    DEBUG_ASSERT(mPtr + i < mEnd);
    return mPtr[i];
  }

  friend bool operator==(const AlignedString& a, const AlignedString& b) noexcept
  {
    return a.str() == b.str();
  }

  friend bool operator!=(const AlignedString& a, const AlignedString& b) noexcept
  {
    return a.str() != b.str();
  }

private:
  char* mPtr { nullptr };
  char* mEnd { nullptr };
};

} // namespace fzx
