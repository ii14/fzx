#pragma once

#include <utility>
#include <cstdint>

namespace fzx {

/// Because libc's toupper can't be inlined
constexpr uint8_t toUpper(uint8_t ch)
{
  return ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
}

/// Because libc's tolower can't be inlined
constexpr uint8_t toLower(uint8_t ch)
{
  return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

/// Round an integer up to the nearest power of two
template <typename T>
static constexpr auto roundPow2(T x) noexcept
{
  --x;
  for (unsigned i = 1; i < sizeof(x) * 8; i *= 2)
    x |= x >> i;
  return ++x;
}

using std::swap;

} // namespace fzx
