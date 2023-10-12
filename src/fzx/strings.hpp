// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cstddef>
#include <cstdint>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/simd.hpp"

namespace fzx {

[[nodiscard]] constexpr uint8_t toUpper(uint8_t ch) noexcept
{
  return ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
}

[[nodiscard]] constexpr uint8_t toLower(uint8_t ch) noexcept
{
  return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

/// `in` is expected to be overallocated with at least fzx::kOveralloc bytes beyond `len`
inline void toLower(char* RESTRICT out, const char* RESTRICT in, size_t len) noexcept
{
#if defined(FZX_AVX2)
  static_assert(fzx::kOveralloc >= 32);
  for (size_t i = 0; i < len; i += 32) {
    auto s = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
    s = simd::toLower(s);
    simd::store(out + i, s);
  }
#elif defined(FZX_SSE2)
  static_assert(fzx::kOveralloc >= 16);
  for (size_t i = 0; i < len; i += 16) {
    auto s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
    s = simd::toLower(s);
    simd::store(out + i, s);
  }
#elif defined(FZX_NEON)
  static_assert(fzx::kOveralloc >= 16);
  for (size_t i = 0; i < len; i += 16) {
    auto s = vld1q_u8(reinterpret_cast<const uint8_t*>(in) + i);
    s = simd::toLower(s);
    simd::store(out + i, s);
  }
#else
  for (size_t i = 0; i < len; ++i)
    out[i] = static_cast<char>(toLower(in[i]));
#endif
}

} // namespace fzx
