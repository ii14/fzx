#pragma once

#include <cstring>
#include <new>
#include <string_view>
#include <utility>

#include "fzx/config.hpp"
#include "fzx/util.hpp"

namespace fzx {

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
    mData = new (kAlign) char[roundUp<kCacheLine>(str.size())];
    std::memcpy(mData, str.data(), str.size());
    mSize = str.size();
  }

  AlignedString(AlignedString&& b) noexcept
    : mData(std::exchange(b.mData, nullptr))
    , mSize(std::exchange(b.mSize, 0))
  {
  }

  AlignedString& operator=(AlignedString&& b) noexcept
  {
    if (this == &b)
      return *this;
    mData = std::exchange(b.mData, nullptr);
    mSize = std::exchange(b.mSize, 0);
    return *this;
  }

  ~AlignedString() noexcept
  {
    operator delete[](mData, kAlign);
  }

  void clear() noexcept
  {
    operator delete[](mData, kAlign);
    mData = nullptr;
    mSize = 0;
  }

  // not going to bother implementing copy, won't be used
  AlignedString(const AlignedString&) = delete;
  AlignedString& operator=(const AlignedString&) = delete;

  [[nodiscard]] bool empty() const noexcept { return mSize == 0; }
  [[nodiscard]] const char* data() const noexcept { return mData; }
  [[nodiscard]] size_t size() const noexcept { return mSize; }
  [[nodiscard]] std::string_view str() const noexcept { return { mData, mSize }; }

private:
  char* mData { nullptr };
  size_t mSize { 0 };
};

} // namespace fzx
