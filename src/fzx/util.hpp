// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace fzx {

/// Because libc's toupper can't be inlined
[[nodiscard]] constexpr uint8_t toUpper(uint8_t ch) noexcept
{
  return ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
}

/// Because libc's tolower can't be inlined
[[nodiscard]] constexpr uint8_t toLower(uint8_t ch) noexcept
{
  return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

/// Check if integer is a power of two
template <typename T>
[[nodiscard]] constexpr bool isPow2(T n) noexcept
{
  static_assert(std::is_integral_v<T>);
  return n > 0 && (n & (n - 1)) == 0;
}

/// Round an integer up to the nearest power of two
template <typename T>
[[nodiscard]] constexpr auto roundPow2(T n) noexcept
{
  static_assert(std::is_integral_v<T>);
  --n;
  for (unsigned i = 1; i < sizeof(n) * 8; i *= 2)
    n |= n >> i;
  return ++n;
}

/// Check if integer is a multiple of N, where N is a power of two
template <size_t N, typename T>
[[nodiscard]] constexpr bool isMulOf(T n) noexcept
{
  static_assert(std::is_integral_v<T>);
  static_assert(isPow2(N));
  return (n & (N - 1)) == 0;
}

/// Round an integer up to a multiple of N, where N is a power of two
template <size_t N, typename T>
[[nodiscard]] constexpr T roundUp(T n) noexcept
{
  static_assert(std::is_integral_v<T>);
  static_assert(isPow2(N));
  constexpr size_t mask = N - 1;
  return (n & ~mask) + static_cast<T>(!isMulOf<N>(n)) * N;
}

using std::swap;

} // namespace fzx
