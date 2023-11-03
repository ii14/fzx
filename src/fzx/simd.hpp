// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#if defined(FZX_SSE2)
# include <emmintrin.h>
#endif
#if defined(FZX_SSE41)
# include <smmintrin.h>
#endif
#if defined(FZX_AVX) || defined(FZX_AVX2)
# include <immintrin.h>
#endif
#if defined(FZX_NEON)
# include <arm_neon.h>
#endif

#include <cstddef>

#include "fzx/macros.hpp"

namespace fzx::simd {

#if defined(FZX_SSE2)
template <typename T>
INLINE void store(T* p, const __m128i& r) noexcept
{
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), r);
}

[[nodiscard]] INLINE __m128i toLower(const __m128i& r) noexcept
{
  auto t = _mm_add_epi8(r, _mm_set1_epi8(63)); // Offset so that A == SCHAR_MIN
  t = _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(-102)), t); // Lower or equal Z
  t = _mm_and_si128(t, _mm_set1_epi8(32)); // Mask lowercase offset
  return _mm_add_epi8(r, t); // Apply offset
}

/// c ? a : b
[[nodiscard]] INLINE __m128 blendv(const __m128& c, const __m128& a, const __m128& b)
{
# if defined(FZX_SSE41)
  return _mm_blendv_ps(b, a, c);
# else
  return _mm_or_ps(_mm_and_ps(a, c), _mm_andnot_ps(c, b));
# endif
}

[[nodiscard]] INLINE float extractv(const __m128& r, unsigned n)
{
# if defined(__GNUC__) || defined(__clang__)
  return r[n];
# else
  // I have no idea what's the best way to do this.
  float t[4];
  _mm_store_ps(t, r);
  return t[n];
# endif
}
#endif // defined(FZX_SSE2)

#if defined(FZX_AVX2)
template <typename T>
INLINE void store(T* p, const __m256i& r) noexcept
{
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), r);
}

[[nodiscard]] INLINE __m256i toLower(const __m256i& r) noexcept
{
  auto t = _mm256_add_epi8(r, _mm256_set1_epi8(63)); // Offset so that A == SCHAR_MIN
  t = _mm256_cmpgt_epi8(_mm256_set1_epi8(static_cast<char>(-102)), t); // Lower or equal Z
  t = _mm256_and_si256(t, _mm256_set1_epi8(32)); // Mask lowercase offset
  return _mm256_add_epi8(r, t); // Apply offset
}
#endif // defined(FZX_AVX2)

#if defined(FZX_NEON)
template <typename T>
INLINE void store(T* p, const uint8x16_t& r) noexcept
{
  vst1q_u8(reinterpret_cast<uint8_t*>(p), r);
}

[[nodiscard]] INLINE uint8x16_t toLower(const uint8x16_t& r) noexcept
{
  auto t = vsubq_u8(r, vmovq_n_u8('A')); // Offset so that A == 0
  t = vcltq_u8(t, vmovq_n_u8(26)); // Lower or equal Z
  t = vandq_u8(t, vmovq_n_u8(32)); // Mask lowercase offset
  return vaddq_u8(r, t); // Apply offset
}

[[nodiscard]] INLINE float extractv(const float32x4_t& r, unsigned n)
{
# if defined(__GNUC__) || defined(__clang__)
  return r[n];
# else
#  error "TODO"
# endif
}
#endif // defined(FZX_NEON)

} // namespace fzx::simd
