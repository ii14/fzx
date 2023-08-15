#pragma once

#if defined(FZX_SSE2)
# include <emmintrin.h>
#endif
#if defined(FZX_AVX2)
# include <immintrin.h>
#endif

#include "fzx/macros.hpp"

namespace fzx::simd {

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

#if defined(FZX_SSE2)
template <typename T>
[[nodiscard]] ALWAYS_INLINE inline __m128i load128i(const T* p) noexcept
{
  return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
}

template <typename T>
ALWAYS_INLINE inline void store(T* p, const __m128i& r) noexcept
{
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), r);
}

[[nodiscard]] ALWAYS_INLINE inline __m128i toLower(const __m128i& r) noexcept
{
  auto t = _mm_add_epi8(r, _mm_set1_epi8(63)); // offset so that A == SCHAR_MIN
  t = _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(-102)), t); // lower or equal Z
  t = _mm_and_si128(t, _mm_set1_epi8(32)); // mask lowercase offset
  return _mm_add_epi8(r, t); // apply offset
}
#endif

#if defined(FZX_AVX2)
template <typename T>
[[nodiscard]] ALWAYS_INLINE inline __m256i load256i(const T* p) noexcept
{
  return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
}

template <typename T>
ALWAYS_INLINE inline void store(T* p, const __m256i& r) noexcept
{
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), r);
}

[[nodiscard]] ALWAYS_INLINE inline __m256i toLower(const __m256i& r) noexcept
{
  auto t = _mm256_add_epi8(r, _mm256_set1_epi8(63)); // offset so that A == SCHAR_MIN
  t = _mm256_cmpgt_epi8(_mm256_set1_epi8(static_cast<char>(-102)), t); // lower or equal Z
  t = _mm256_and_si256(t, _mm256_set1_epi8(32)); // mask lowercase offset
  return _mm256_add_epi8(r, t); // apply offset
}
#endif

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace fzx::simd
