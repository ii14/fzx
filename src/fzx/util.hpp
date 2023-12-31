// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

#include "fzx/macros.hpp"

namespace fzx {

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

template <typename T>
[[nodiscard]] inline T load(const void* p)
{
  static_assert(std::is_trivially_constructible_v<T>);
  static_assert(std::is_trivially_copyable_v<T>);
  T v; // NOLINT(cppcoreguidelines-init-variables)
  std::memcpy(&v, p, sizeof(T));
  return v;
}

template <typename T>
[[nodiscard]] inline T loadAligned(const void* p)
{
  static_assert(std::is_trivially_constructible_v<T>);
  static_assert(std::is_trivially_copyable_v<T>);
  T v; // NOLINT(cppcoreguidelines-init-variables)
  std::memcpy(&v, ASSUME_ALIGNED(p, alignof(T)), sizeof(T));
  return v;
}

template <typename T>
inline void store(void* p, const T& v)
{
  static_assert(std::is_trivially_copyable_v<T>);
  std::memcpy(p, &v, sizeof(T));
}

template <typename T>
inline void storeAligned(void* p, const T& v)
{
  static_assert(std::is_trivially_copyable_v<T>);
  std::memcpy(ASSUME_ALIGNED(p, alignof(T)), &v, sizeof(T));
}

// NOLINTBEGIN(google-runtime-int)

/// Find-first-set
[[nodiscard]] inline int ffs32(uint32_t x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
  static_assert(sizeof(int) == 4);
  return __builtin_ffs(static_cast<int>(x));
#elif defined(_MSC_VER)
  unsigned long index;
  return static_cast<int>(_BitScanForward(&index, static_cast<unsigned long>(x)) ? index + 1 : 0);
#else
# error "32-bit find-first-set is not available"
  // TODO: portable implementation
#endif
}

/// Find-first-set
[[nodiscard]] inline int ffs64(uint64_t x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
  static_assert(sizeof(long) == 8);
  return __builtin_ffsl(static_cast<long>(x));
#elif defined(_MSC_VER)
  unsigned long index;
  return static_cast<int>(_BitScanForward64(&index, static_cast<__int64>(x)) ? index + 1 : 0);
#else
# error "64-bit find-first-set is not available"
  // TODO: portable implementation
#endif
}

// NOLINTEND(google-runtime-int)

inline void* alignedAlloc(size_t alignment, size_t size)
{
#if defined(_WIN32)
  return _aligned_malloc(size, alignment);
#else
  return std::aligned_alloc(alignment, size);
#endif
}

inline void alignedFree(void* ptr)
{
#if defined(_WIN32)
  return _aligned_free(ptr);
#else
  return std::free(ptr);
#endif
}

using std::swap;

} // namespace fzx
