#pragma once

#include <cstring>
#include <new>
#include <string_view>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/util.hpp"
#include "fzx/macros.hpp"

namespace fzx {

/// Read-only string that is guaranteed to be aligned to the cache line size.
/// The underlying memory is overallocated to a multiple of the cache line size.
struct AlignedString
{
private:
  static constexpr std::align_val_t kAlign { kCacheLine };

public:
  constexpr AlignedString() noexcept = default;

  AlignedString(std::string_view str)
  {
    if (str.empty())
      return;
    mPtr = new (kAlign) char[roundUp<kCacheLine>(str.size())];
    std::memcpy(mPtr, str.data(), str.size());
    mEnd = mPtr + str.size();
  }

  AlignedString(AlignedString&& b) noexcept
    : mPtr(std::exchange(b.mPtr, nullptr))
    , mEnd(std::exchange(b.mEnd, nullptr))
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

  ~AlignedString() noexcept
  {
    operator delete[](mPtr, kAlign);
  }

  void clear() noexcept
  {
    operator delete[](mPtr, kAlign);
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

  [[nodiscard]] const char* begin() const noexcept { return mPtr; }
  [[nodiscard]] const char* end() const noexcept { return mEnd; }

  [[nodiscard]] char operator[](ptrdiff_t i) const noexcept
  {
    DEBUG_ASSERT(i >= 0);
    DEBUG_ASSERT(mPtr != nullptr);
    DEBUG_ASSERT(mPtr + i < mEnd);
    return mPtr[i];
  }

private:
  char* mPtr { nullptr };
  char* mEnd { nullptr };
};

} // namespace fzx