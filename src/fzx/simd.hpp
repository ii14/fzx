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

#include <cstddef>

#include "fzx/macros.hpp"

namespace fzx::simd {

#if defined(FZX_SSE2)
template <typename T>
[[nodiscard]] INLINE __m128i load128i(const T* p) noexcept
{
  return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
}

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

[[nodiscard]] INLINE __m128 blendv(const __m128& a, const __m128& b, const __m128& c)
{
# if defined(FZX_SSE41)
  return _mm_blendv_ps(a, b, c);
# else
  return _mm_or_ps(_mm_and_ps(b, c), _mm_andnot_ps(c, a));
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
[[nodiscard]] INLINE __m256i load256i(const T* p) noexcept
{
  return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
}

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

} // namespace fzx::simd
