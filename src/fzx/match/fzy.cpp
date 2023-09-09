// Licensed under LGPLv3 - see LICENSE file for details.
//
// This file incorporates work covered by the following copyright and permission notice:
//
// The MIT License (MIT)
//
// Copyright (c) 2014 John Hawthorn
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "fzx/match/fzy.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/match/fzy_config.hpp"
#include "fzx/simd.hpp"
#include "fzx/util.hpp"

#include <immintrin.h> // TODO: temporary

// TODO: clean this up

namespace fzx::fzy {

namespace {

constexpr auto kBonusStates = []{
  std::array<std::array<Score, 256>, 3> r {};

  r[1]['/'] = kScoreMatchSlash;
  r[1]['-'] = kScoreMatchWord;
  r[1]['_'] = kScoreMatchWord;
  r[1][' '] = kScoreMatchWord;
  r[1]['.'] = kScoreMatchDot;

  r[2]['/'] = kScoreMatchSlash;
  r[2]['-'] = kScoreMatchWord;
  r[2]['_'] = kScoreMatchWord;
  r[2][' '] = kScoreMatchWord;
  r[2]['.'] = kScoreMatchDot;
  for (char i = 'a'; i <= 'z'; ++i)
    r[2][i] = kScoreMatchCapital;

  return r;
}();

constexpr auto kBonusIndex = []{
  std::array<uint8_t, 256> r {};
  for (char i = 'A'; i <= 'Z'; ++i)
    r[i] = 2;
  for (char i = 'a'; i <= 'z'; ++i)
    r[i] = 1;
  for (char i = '0'; i <= '9'; ++i)
    r[i] = 1;
  return r;
}();

/// `in` is expected to be overallocated with at least fzx::kOveralloc bytes beyond `len`
void toLowercase(const char* RESTRICT in, size_t len, char* RESTRICT out) noexcept
{
  // TODO: neon
#if defined(FZX_AVX2)
  static_assert(fzx::kOveralloc >= 32);
  for (size_t i = 0; i < len; i += 32) {
    auto s = simd::load256i(in + i);
    s = simd::toLower(s);
    simd::store(out + i, s);
  }
#elif defined(FZX_SSE2)
  static_assert(fzx::kOveralloc >= 16);
  for (size_t i = 0; i < len; i += 16) {
    auto s = simd::load128i(in + i);
    s = simd::toLower(s);
    simd::store(out + i, s);
  }
#else
  for (size_t i = 0; i < len; ++i)
    out[i] = static_cast<char>(toLower(in[i]));
#endif
}

} // namespace

bool hasMatch2(std::string_view needle, std::string_view haystack) noexcept
{
  // TODO: neon
#if (defined(FZX_AVX2) || defined(FZX_SSE2)) && FZX_HAS_BUILTIN(__builtin_ffsl)
  // Interestingly, the original fzy doesn't seem to have any problems with performance here.
  // If I had to guess, it's probably because it uses strpbrk, which is probably optimized
  // already in glibc? We're not using null terminated strings though.
  const char* np = needle.begin();
  const char* hp = haystack.begin();
  const char* const nEnd = needle.end();
  const char* const hEnd = haystack.end();
  for (;;) {
    if (np == nEnd)
      return true;
    if (hp >= hEnd)
      return false;
    static_assert(fzx::kOveralloc >= 32);
# if defined(FZX_AVX2)
    auto n = _mm256_set1_epi8(static_cast<char>(toLower(*np)));
    auto h1 = simd::load256i(hp);
    h1 = simd::toLower(h1);
    uint32_t r1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(h1, n));
# else // defined(FZX_SSE2)
    auto n = _mm_set1_epi8(static_cast<char>(toLower(*np)));
    auto h1 = simd::load128i(hp);
    auto h2 = simd::load128i(hp + 16);
    h1 = simd::toLower(h1);
    h2 = simd::toLower(h2);
    uint32_t r1 = _mm_movemask_epi8(_mm_cmpeq_epi8(h1, n));
    uint32_t r2 = _mm_movemask_epi8(_mm_cmpeq_epi8(h2, n));
    r1 = r1 | (r2 << 16);
# endif
    ptrdiff_t d = hEnd - hp; // How many bytes are left in the haystack until the end
    r1 &= d < 32 ? 0xFFFFFFFFU >> (32 - d) : 0xFFFFFFFFU; // Mask out positions out of bounds
    hp += r1 ? __builtin_ffsl(r1) : 32; // Skip to after the first matched character
    np += static_cast<bool>(r1); // Next needle character if we had a match
  }
#else
  const char* it = haystack.begin();
  for (char ch : needle) {
    char uch = static_cast<char>(toUpper(ch));
    while (it != haystack.end() && *it != ch && *it != uch)
      ++it;
    if (it == haystack.end())
      return false;
    ++it;
  }
  return true;
#endif
}

bool hasMatch(std::string_view needle, std::string_view haystack) noexcept
{
  // technically needle and haystack never will be empty,
  // so maybe fix the tests and turn it into debug asserts
  if (needle.empty())
    return true;
  if (haystack.empty()) // otherwise can lead to end < it
    return false;

  constexpr ptrdiff_t kWidth { 16 };
  static_assert(isPow2(kWidth)); // -1 has to yield a mask for unaligned bytes
  constexpr uintptr_t kMisalignment { kWidth - 1 };

  const char* nit = needle.begin();
  const char* nend = needle.end();
  const char* it = haystack.begin();
  const char* end = haystack.end();

  auto offsetIt = reinterpret_cast<uintptr_t>(it) & kMisalignment;
  auto offsetEnd = reinterpret_cast<uintptr_t>(end) & kMisalignment;
  it -= offsetIt;
  if (offsetEnd == 0)
    offsetEnd = kWidth;
  end -= offsetEnd;
  DEBUG_ASSERT(end >= it);

  const auto maskEnd = ~(uint32_t{0xFFFF} << offsetEnd);

  auto xmm0 = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(it)));
  auto xmm1 = _mm_set1_epi8(static_cast<char>(toLower(nit[0])));

  for (;;) {
    uint32_t r = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));
    r &= uint32_t{0xFFFF} << offsetIt;
    if (it == end)
      r &= maskEnd;

    if (int fs = __builtin_ffs(static_cast<int>(r)); fs != 0) {
      if (++nit == nend)
        return true;
      xmm1 = _mm_set1_epi8(static_cast<char>(toLower(nit[0])));
      if (fs != 16) {
        offsetIt = fs;
        continue;
      }
    }

    if (it == end)
      return false;
    it += kWidth;
    xmm0 = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(it)));
    offsetIt = 0;
  }
}

namespace {

void precomputeBonus(std::string_view haystack, Score* matchBonus) noexcept
{
  // Which positions are beginning of words
  uint8_t lastCh = '/';
  for (size_t i = 0; i < haystack.size(); ++i) {
    uint8_t ch = haystack[i];
    matchBonus[i] = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;
  }
}

using ScoreArray = std::array<Score, kMatchMaxLen>;

struct MatchStruct
{
  int mNeedleLen;
  int mHaystackLen;

  char mLowerNeedle[kMatchMaxLen];
  char mLowerHaystack[kMatchMaxLen];

  Score mMatchBonus[kMatchMaxLen];

  MatchStruct(std::string_view needle, std::string_view haystack) noexcept;

  inline void matchRow(
    int row,
    Score* RESTRICT currD,
    Score* RESTRICT currM,
    const Score* RESTRICT lastD,
    const Score* RESTRICT lastM) noexcept;
};

// "initialize all your variables" they said.
// and now memset takes up 20% of the runtime of your program.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
MatchStruct::MatchStruct(std::string_view needle, std::string_view haystack) noexcept
  : mNeedleLen(static_cast<int>(needle.size()))
  , mHaystackLen(static_cast<int>(haystack.size()))
{
  if (mHaystackLen > kMatchMaxLen || mNeedleLen > mHaystackLen)
    return;

  // TODO: needle can be preprocessed only once
  toLowercase(needle.data(), mNeedleLen, mLowerNeedle);
  toLowercase(haystack.data(), mHaystackLen, mLowerHaystack);

  precomputeBonus(haystack, mMatchBonus);
}

void MatchStruct::matchRow(
    int row,
    Score* RESTRICT currD,
    Score* RESTRICT currM,
    const Score* RESTRICT lastD,
    const Score* RESTRICT lastM) noexcept
{
  Score prevScore = kScoreMin;
  Score gapScore = row == mNeedleLen - 1 ? kScoreGapTrailing : kScoreGapInner;

  for (int i = 0; i < mHaystackLen; ++i) {
    if (mLowerNeedle[row] == mLowerHaystack[i]) {
      Score score = kScoreMin;
      if (!row) {
        score = (i * kScoreGapLeading) + mMatchBonus[i];
      } else if (i) { // row > 0 && i > 0
        score = std::max(
            lastM[i - 1] + mMatchBonus[i],
            // consecutive match, doesn't stack with matchBonus
            lastD[i - 1] + kScoreMatchConsecutive);
      }
      currD[i] = score;
      currM[i] = prevScore = std::max(score, prevScore + gapScore);
    } else {
      currD[i] = kScoreMin;
      currM[i] = prevScore = prevScore + gapScore;
    }
  }
}

} // namespace

Score match(std::string_view needle, std::string_view haystack) noexcept
{
  if (needle.empty())
    return kScoreMin;

  if (haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    // Unreasonably large candidate: return no score
    // If it is a valid match it will still be returned, it will
    // just be ranked below any reasonably sized candidates
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    // Since this method can only be called with a haystack which
    // matches needle. If the lengths of the strings are equal the
    // strings themselves must also be equal (ignoring case).
    return kScoreMax;
  }

  MatchStruct match { needle, haystack };

  // D[][] Stores the best score for this position ending with a match.
  // M[][] Stores the best possible score at this position.
  ScoreArray D[2];
  ScoreArray M[2];

  Score* lastD = D[0].data();
  Score* lastM = M[0].data();
  Score* currD = D[1].data();
  Score* currM = M[1].data();

  for (int i = 0; i < match.mNeedleLen; ++i) {
    match.matchRow(i, currD, currM, lastD, lastM);
    std::swap(currD, lastD);
    std::swap(currM, lastM);
  }

  return lastM[match.mHaystackLen - 1];
}

Score match1(std::string_view needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() == 1);
  if (haystack.size() > kMatchMaxLen || haystack.empty()) {
    return kScoreMin;
  } else if (haystack.size() == 1) {
    return kScoreMax;
  }

  const int haystackLen = static_cast<int>(haystack.size());
  const uint8_t lowerNeedle = toLower(needle[0]);
  uint8_t lastCh = '/';
  Score score = kScoreMin;
  {
    uint8_t ch = haystack[0];
    Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;
    if (lowerNeedle == toLower(ch))
      score = bonus;
  }
  for (int i = 1; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;
    score += kScoreGapTrailing;
    if (lowerNeedle == toLower(ch))
      if (Score ns = (static_cast<float>(i) * kScoreGapLeading) + bonus; ns > score)
        score = ns;
  }
  return score;
}

namespace {
alignas(64) constexpr Score kScoreGapTable[7] {
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapTrailing,
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapTrailing,
};
} // namespace

Score matchSse4(std::string_view needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() <= 4);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  // const int needleLen = static_cast<int>(needle.size());
  const int haystackLen = static_cast<int>(haystack.size());

  const auto kMin = _mm_set1_ps(kScoreMin);
  const auto kGap = _mm_loadu_ps(&kScoreGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = _mm_set1_ps(kScoreMatchConsecutive);
  const auto kZero = _mm_setzero_si128();

  auto nt = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(needle.data())));
  nt = _mm_unpacklo_epi8(nt, kZero);
  auto n = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nt, kZero));
  auto d = kMin;
  auto m = kMin;
  uint8_t lastCh = '/';

  const auto kGapLeading = _mm_set_ss(kScoreGapLeading);
  auto gapLeading = _mm_set_ss(0);
  _Pragma("GCC unroll 8")
  for (int i = 0; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    float bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;

    auto r = _mm_set1_ps(toLower(ch));
    auto b = _mm_set1_ps(bonus);
    auto c = _mm_cmpeq_ps(n, r);
    auto pm = _mm_shuffle_ps(m, m, 0b10010011);
    auto pd = _mm_shuffle_ps(d, d, 0b10010011);
    auto s = _mm_max_ps(_mm_add_ps(pm, b), _mm_add_ps(pd, kConsecutive));
    gapLeading = _mm_add_ss(gapLeading, kGapLeading);
    s = _mm_move_ss(s, _mm_add_ss(gapLeading, b));
    auto g = _mm_add_ps(m, kGap);
    d = _mm_blendv_ps(kMin, s, c);
    m = _mm_blendv_ps(g, _mm_max_ps(s, g), c);
  }

  return m[(needle.size() + 3) & 0b11];
}

Score matchSse8(std::string_view needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() <= 8);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  // const int needleLen = static_cast<int>(needle.size());
  const int haystackLen = static_cast<int>(haystack.size());

  const auto kMin = _mm_set1_ps(kScoreMin);
  const auto kGap1 = _mm_set1_ps(kScoreGapInner);
  const auto kGap2 = _mm_loadu_ps(&kScoreGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = _mm_set1_ps(kScoreMatchConsecutive);
  const auto kZero = _mm_setzero_si128();

  auto nt = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(needle.data())));
  nt = _mm_unpacklo_epi8(nt, kZero);
  auto n1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nt, kZero));
  auto n2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(nt, kZero));
  auto d1 = kMin;
  auto d2 = kMin;
  auto m1 = kMin;
  auto m2 = kMin;
  uint8_t lastCh = '/';

  const auto kGapLeading = _mm_set_ss(kScoreGapLeading);
  auto gapLeading = _mm_set_ss(0);
  _Pragma("GCC unroll 8")
  for (int i = 0; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    float bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;

    auto r = _mm_set1_ps(static_cast<char>(toLower(ch)));
    auto b = _mm_set1_ps(bonus);
    auto c1 = _mm_cmpeq_ps(n1, r);
    auto c2 = _mm_cmpeq_ps(n2, r);

    auto pm1 = _mm_shuffle_ps(m1, m1, 0b10010011);
    auto pm2 = _mm_shuffle_ps(m2, m2, 0b10010011);
    auto pd1 = _mm_shuffle_ps(d1, d1, 0b10010011);
    auto pd2 = _mm_shuffle_ps(d2, d2, 0b10010011);
    pm2 = _mm_blend_ps(pm2, pm1, 0b0001);
    pd2 = _mm_blend_ps(pd2, pd1, 0b0001);
    auto s1 = _mm_max_ps(_mm_add_ps(pm1, b), _mm_add_ps(pd1, kConsecutive));
    auto s2 = _mm_max_ps(_mm_add_ps(pm2, b), _mm_add_ps(pd2, kConsecutive));
    gapLeading = _mm_add_ss(gapLeading, kGapLeading);
    s1 = _mm_move_ss(s1, _mm_add_ss(gapLeading, b));

    auto g1 = _mm_add_ps(m1, kGap1);
    auto g2 = _mm_add_ps(m2, kGap2);
    d1 = _mm_blendv_ps(kMin, s1, c1);
    d2 = _mm_blendv_ps(kMin, s2, c2);
    m1 = _mm_blendv_ps(g1, _mm_max_ps(s1, g1), c1);
    m2 = _mm_blendv_ps(g2, _mm_max_ps(s2, g2), c2);
  }

  return m2[(needle.size() + 3) & 0b11];
}

Score matchSse12(std::string_view needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() <= 12);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  // const int needleLen = static_cast<int>(needle.size());
  const int haystackLen = static_cast<int>(haystack.size());

  const auto kMin = _mm_set1_ps(kScoreMin);
  const auto kGap1 = _mm_set1_ps(kScoreGapInner);
  const auto kGap2 = _mm_loadu_ps(&kScoreGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = _mm_set1_ps(kScoreMatchConsecutive);
  const auto kZero = _mm_setzero_si128();

  auto nt = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(needle.data())));
  auto nlo = _mm_unpacklo_epi8(nt, kZero);
  auto nhi = _mm_unpackhi_epi8(nt, kZero);
  auto n1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nlo, kZero));
  auto n2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(nlo, kZero));
  auto n3 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nhi, kZero));
  auto d1 = kMin;
  auto d2 = kMin;
  auto d3 = kMin;
  auto m1 = kMin;
  auto m2 = kMin;
  auto m3 = kMin;
  uint8_t lastCh = '/';

  const auto kGapLeading = _mm_set_ss(kScoreGapLeading);
  auto gapLeading = _mm_set_ss(0);
  _Pragma("GCC unroll 4")
  for (int i = 0; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    float bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;

    auto r = _mm_set1_ps(static_cast<char>(toLower(ch)));
    auto b = _mm_set1_ps(bonus);
    auto c1 = _mm_cmpeq_ps(n1, r);
    auto c2 = _mm_cmpeq_ps(n2, r);
    auto c3 = _mm_cmpeq_ps(n3, r);

    auto pm1 = _mm_shuffle_ps(m1, m1, 0b10010011);
    auto pm2 = _mm_shuffle_ps(m2, m2, 0b10010011);
    auto pm3 = _mm_shuffle_ps(m3, m3, 0b10010011);
    auto pd1 = _mm_shuffle_ps(d1, d1, 0b10010011);
    auto pd2 = _mm_shuffle_ps(d2, d2, 0b10010011);
    auto pd3 = _mm_shuffle_ps(d3, d3, 0b10010011);
    pm3 = _mm_blend_ps(pm3, pm2, 0b0001);
    pd3 = _mm_blend_ps(pd3, pd2, 0b0001);
    pm2 = _mm_blend_ps(pm2, pm1, 0b0001);
    pd2 = _mm_blend_ps(pd2, pd1, 0b0001);
    auto s1 = _mm_max_ps(_mm_add_ps(pm1, b), _mm_add_ps(pd1, kConsecutive));
    auto s2 = _mm_max_ps(_mm_add_ps(pm2, b), _mm_add_ps(pd2, kConsecutive));
    auto s3 = _mm_max_ps(_mm_add_ps(pm3, b), _mm_add_ps(pd3, kConsecutive));
    gapLeading = _mm_add_ss(gapLeading, kGapLeading);
    s1 = _mm_move_ss(s1, _mm_add_ss(gapLeading, b));

    auto g1 = _mm_add_ps(m1, kGap1);
    auto g2 = _mm_add_ps(m2, kGap1);
    auto g3 = _mm_add_ps(m3, kGap2);
    d1 = _mm_blendv_ps(kMin, s1, c1);
    d2 = _mm_blendv_ps(kMin, s2, c2);
    d3 = _mm_blendv_ps(kMin, s3, c3);
    m1 = _mm_blendv_ps(g1, _mm_max_ps(s1, g1), c1);
    m2 = _mm_blendv_ps(g2, _mm_max_ps(s2, g2), c2);
    m3 = _mm_blendv_ps(g3, _mm_max_ps(s3, g3), c3);
  }

  return m3[(needle.size() + 3) & 0b11];
}

Score matchSse16(std::string_view needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() <= 16);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  // const int needleLen = static_cast<int>(needle.size());
  const int haystackLen = static_cast<int>(haystack.size());

  const auto kMin = _mm_set1_ps(kScoreMin);
  const auto kGap1 = _mm_set1_ps(kScoreGapInner);
  const auto kGap2 = _mm_loadu_ps(&kScoreGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = _mm_set1_ps(kScoreMatchConsecutive);
  const auto kZero = _mm_setzero_si128();

  auto nt = _mm_load_si128(reinterpret_cast<const __m128i*>(needle.data()));
  nt = simd::toLower(nt);
  auto nlo = _mm_unpacklo_epi8(nt, kZero);
  auto nhi = _mm_unpackhi_epi8(nt, kZero);
  auto n1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nlo, kZero));
  auto n2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(nlo, kZero));
  auto n3 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(nhi, kZero));
  auto n4 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(nhi, kZero));
  auto d1 = kMin;
  auto d2 = kMin;
  auto d3 = kMin;
  auto d4 = kMin;
  auto m1 = kMin;
  auto m2 = kMin;
  auto m3 = kMin;
  auto m4 = kMin;
  uint8_t lastCh = '/';

  const auto kGapLeading = _mm_set_ss(kScoreGapLeading);
  auto gapLeading = _mm_set_ss(0);
  _Pragma("GCC unroll 4")
  for (int i = 0; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    float bonus = kBonusStates[kBonusIndex[ch]][lastCh];
    lastCh = ch;

    auto r = _mm_set1_ps(static_cast<char>(toLower(ch)));
    auto b = _mm_set1_ps(bonus);
    auto c1 = _mm_cmpeq_ps(n1, r);
    auto c2 = _mm_cmpeq_ps(n2, r);
    auto c3 = _mm_cmpeq_ps(n3, r);
    auto c4 = _mm_cmpeq_ps(n4, r);

    auto pm1 = _mm_shuffle_ps(m1, m1, 0b10010011);
    auto pm2 = _mm_shuffle_ps(m2, m2, 0b10010011);
    auto pm3 = _mm_shuffle_ps(m3, m3, 0b10010011);
    auto pm4 = _mm_shuffle_ps(m4, m4, 0b10010011);
    auto pd1 = _mm_shuffle_ps(d1, d1, 0b10010011);
    auto pd2 = _mm_shuffle_ps(d2, d2, 0b10010011);
    auto pd3 = _mm_shuffle_ps(d3, d3, 0b10010011);
    auto pd4 = _mm_shuffle_ps(d4, d4, 0b10010011);
    pm4 = _mm_blend_ps(pm4, pm3, 0b0001);
    pd4 = _mm_blend_ps(pd4, pd3, 0b0001);
    pm3 = _mm_blend_ps(pm3, pm2, 0b0001);
    pd3 = _mm_blend_ps(pd3, pd2, 0b0001);
    pm2 = _mm_blend_ps(pm2, pm1, 0b0001);
    pd2 = _mm_blend_ps(pd2, pd1, 0b0001);
    auto s1 = _mm_max_ps(_mm_add_ps(pm1, b), _mm_add_ps(pd1, kConsecutive));
    auto s2 = _mm_max_ps(_mm_add_ps(pm2, b), _mm_add_ps(pd2, kConsecutive));
    auto s3 = _mm_max_ps(_mm_add_ps(pm3, b), _mm_add_ps(pd3, kConsecutive));
    auto s4 = _mm_max_ps(_mm_add_ps(pm4, b), _mm_add_ps(pd4, kConsecutive));
    gapLeading = _mm_add_ss(gapLeading, kGapLeading);
    s1 = _mm_move_ss(s1, _mm_add_ss(gapLeading, b));

    auto g1 = _mm_add_ps(m1, kGap1);
    auto g2 = _mm_add_ps(m2, kGap1);
    auto g3 = _mm_add_ps(m3, kGap1);
    auto g4 = _mm_add_ps(m4, kGap2);
    d1 = _mm_blendv_ps(kMin, s1, c1);
    d2 = _mm_blendv_ps(kMin, s2, c2);
    d3 = _mm_blendv_ps(kMin, s3, c3);
    d4 = _mm_blendv_ps(kMin, s4, c4);
    m1 = _mm_blendv_ps(g1, _mm_max_ps(s1, g1), c1);
    m2 = _mm_blendv_ps(g2, _mm_max_ps(s2, g2), c2);
    m3 = _mm_blendv_ps(g3, _mm_max_ps(s3, g3), c3);
    m4 = _mm_blendv_ps(g4, _mm_max_ps(s4, g4), c4);
  }

  return m4[(needle.size() + 3) & 0b11];
}

Score matchPositions(std::string_view needle, std::string_view haystack, Positions* positions)
{
  if (needle.empty())
    return kScoreMin;

  MatchStruct match { needle, haystack };

  const int n = match.mNeedleLen;
  const int m = match.mHaystackLen;

  if (m > kMatchMaxLen || n > m) {
    // Unreasonably large candidate: return no score
    // If it is a valid match it will still be returned, it will
    // just be ranked below any reasonably sized candidates
    return kScoreMin;
  } else if (n == m) {
    // Since this method can only be called with a haystack which
    // matches needle. If the lengths of the strings are equal the
    // strings themselves must also be equal (ignoring case).
    if (positions)
      for (int i = 0; i < n; ++i)
        (*positions)[i] = i;
    return kScoreMax;
  }

  // D[][] Stores the best score for this position ending with a match.
  // M[][] Stores the best possible score at this position.
  std::vector<ScoreArray> D;
  std::vector<ScoreArray> M;
  D.resize(n);
  M.resize(n);

  Score* lastD = nullptr;
  Score* lastM = nullptr;
  Score* currD = nullptr;
  Score* currM = nullptr;

  for (int i = 0; i < n; ++i) {
    currD = D[i].data();
    currM = M[i].data();

    match.matchRow(i, currD, currM, lastD, lastM);

    lastD = currD;
    lastM = currM;
  }

  // backtrace to find the positions of optimal matching
  if (positions) {
    bool matchRequired = false;
    for (int i = n - 1, j = m - 1; i >= 0; --i) {
      for (; j >= 0; --j) {
        // There may be multiple paths which result in
        // the optimal weight.
        //
        // For simplicity, we will pick the first one
        // we encounter, the latest in the candidate
        // string.
        if (D[i][j] != kScoreMin && (matchRequired || D[i][j] == M[i][j])) {
          // If this score was determined using
          // kScoreMatchConsecutive, the
          // previous character MUST be a match
          matchRequired =
              i && j &&
              M[i][j] == D[i - 1][j - 1] + kScoreMatchConsecutive;
          (*positions)[i] = j--;
          break;
        }
      }
    }
  }

  Score result = M[n - 1][m - 1];

  return result;
}

} // namespace fzx::fzy
