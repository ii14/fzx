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

#include "fzx/match/fzy/fzy.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/match/fzy/bonus.hpp"
#include "fzx/match/fzy/config.hpp"
#include "fzx/simd.hpp"
#include "fzx/strings.hpp"
#include "fzx/util.hpp"

namespace fzx::fzy {

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

  MatchStruct(const AlignedString& needle, std::string_view haystack) noexcept;

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
MatchStruct::MatchStruct(const AlignedString& needle, std::string_view haystack) noexcept
  : mNeedleLen(static_cast<int>(needle.size()))
  , mHaystackLen(static_cast<int>(haystack.size()))
{
  if (mHaystackLen > kMatchMaxLen || mNeedleLen > mHaystackLen)
    return;

  // TODO: needle can be preprocessed only once
  toLower(mLowerNeedle, needle.data(), mNeedleLen);
  toLower(mLowerHaystack, haystack.data(), mHaystackLen);

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
        score = (static_cast<Score>(i) * kScoreGapLeading) + mMatchBonus[i];
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

Score score(const AlignedString& needle, std::string_view haystack) noexcept
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
  ScoreArray d[2];
  ScoreArray m[2];

  Score* lastD = d[0].data();
  Score* lastM = m[0].data();
  Score* currD = d[1].data();
  Score* currM = m[1].data();

  for (int i = 0; i < match.mNeedleLen; ++i) {
    match.matchRow(i, currD, currM, lastD, lastM);
    std::swap(currD, lastD);
    std::swap(currM, lastM);
  }

  return lastM[match.mHaystackLen - 1];
}

Score score1(const AlignedString& needle, std::string_view haystack) noexcept
{
  DEBUG_ASSERT(needle.size() == 1);
  if (needle.empty() || haystack.size() > kMatchMaxLen || haystack.empty()) {
    return kScoreMin;
  } else if (haystack.size() == 1) {
    return kScoreMax;
  }

  // Surely this isn't optimal and could be optimized further.

  const int haystackLen = static_cast<int>(haystack.size());
  const uint8_t lowerNeedle = toLower(needle[0]);
  uint8_t lastCh = '/';
  Score score = kScoreMin;

  {
    uint8_t ch = haystack[0];
    if (lowerNeedle == toLower(ch)) {
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      score = bonus;
    }
    lastCh = ch;
  }

  for (int i = 1; i < haystackLen; ++i) {
    uint8_t ch = haystack[i];
    score += kScoreGapTrailing;
    if (lowerNeedle == toLower(ch)) {
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      if (Score ns = (static_cast<Score>(i) * kScoreGapLeading) + bonus; ns > score)
        score = ns;
    }
    lastCh = ch;
  }

  return score;
}

#if defined(FZX_SSE2) || defined(FZX_NEON)
namespace {
alignas(32) constexpr Score kGapTable[7] {
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapTrailing,
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapInner,
  kScoreGapTrailing,
};
} // namespace
#endif

#if defined(FZX_SSE2)
// TODO: variable sized needle

template <size_t N>
Score scoreSSE(const AlignedString& needle, std::string_view haystack) noexcept
{
  static_assert(N == 4 || N == 8 || N == 12 || N == 16);

  DEBUG_ASSERT(needle.size() <= N);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  const int haystackLen = static_cast<int>(haystack.size());

  const auto kZero = _mm_setzero_si128();
  const auto kMin = _mm_set1_ps(kScoreMin);
  const auto kGap1 [[maybe_unused]] = _mm_set1_ps(kScoreGapInner);
  const auto kGap2 = _mm_loadu_ps(&kGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = _mm_set1_ps(kScoreMatchConsecutive);
  const auto kGapLeading = _mm_set_ss(kScoreGapLeading);

  uint8_t lastCh = '/';
  auto g = _mm_set_ss(0); // Leading gap score
  auto nt = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(needle.data())));

  if constexpr (N == 4) {
    nt = _mm_unpacklo_epi8(nt, kZero);
    auto n = _mm_unpacklo_epi16(nt, kZero);
    auto d = kMin;
    auto m = kMin;

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = _mm_set1_epi32(toLower(ch));
      auto b = _mm_set1_ps(bonus);
      auto c = _mm_cmpeq_epi32(n, r);

      auto s = _mm_max_ps(_mm_add_ps(m, b), _mm_add_ps(d, kConsecutive));
      s = _mm_shuffle_ps(s, s, _MM_SHUFFLE(2, 1, 0, 3));
      s = _mm_move_ss(s, _mm_add_ss(g, b));
      g = _mm_add_ss(g, kGapLeading);

      d = simd::blendv(_mm_castsi128_ps(c), s, kMin);
      m = _mm_max_ps(d, _mm_add_ps(m, kGap2));
    }

    return simd::extractv(m, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 8) {
    nt = _mm_unpacklo_epi8(nt, kZero);
    auto n1 = _mm_unpacklo_epi16(nt, kZero);
    auto n2 = _mm_unpackhi_epi16(nt, kZero);
    auto d1 = kMin, d2 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = _mm_set1_epi32(toLower(ch));
      auto b = _mm_set1_ps(bonus);
      auto c1 = _mm_cmpeq_epi32(n1, r);
      auto c2 = _mm_cmpeq_epi32(n2, r);

      auto s1 = _mm_max_ps(_mm_add_ps(m1, b), _mm_add_ps(d1, kConsecutive));
      auto s2 = _mm_max_ps(_mm_add_ps(m2, b), _mm_add_ps(d2, kConsecutive));
      s1 = _mm_shuffle_ps(s1, s1, _MM_SHUFFLE(2, 1, 0, 3));
      s2 = _mm_shuffle_ps(s2, s2, _MM_SHUFFLE(2, 1, 0, 3));
      s2 = _mm_move_ss(s2, s1);
      s1 = _mm_move_ss(s1, _mm_add_ss(g, b));
      g = _mm_add_ss(g, kGapLeading);

      d1 = simd::blendv(_mm_castsi128_ps(c1), s1, kMin);
      d2 = simd::blendv(_mm_castsi128_ps(c2), s2, kMin);
      m1 = _mm_max_ps(d1, _mm_add_ps(m1, kGap1));
      m2 = _mm_max_ps(d2, _mm_add_ps(m2, kGap2));
    }

    return simd::extractv(m2, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 12) {
    auto nlo = _mm_unpacklo_epi8(nt, kZero);
    auto nhi = _mm_unpackhi_epi8(nt, kZero);
    auto n1 = _mm_unpacklo_epi16(nlo, kZero);
    auto n2 = _mm_unpackhi_epi16(nlo, kZero);
    auto n3 = _mm_unpacklo_epi16(nhi, kZero);
    auto d1 = kMin, d2 = kMin, d3 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin, m3 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = _mm_set1_epi32(toLower(ch));
      auto b = _mm_set1_ps(bonus);
      auto c1 = _mm_cmpeq_epi32(n1, r);
      auto c2 = _mm_cmpeq_epi32(n2, r);
      auto c3 = _mm_cmpeq_epi32(n3, r);

      auto s1 = _mm_max_ps(_mm_add_ps(m1, b), _mm_add_ps(d1, kConsecutive));
      auto s2 = _mm_max_ps(_mm_add_ps(m2, b), _mm_add_ps(d2, kConsecutive));
      auto s3 = _mm_max_ps(_mm_add_ps(m3, b), _mm_add_ps(d3, kConsecutive));
      s1 = _mm_shuffle_ps(s1, s1, _MM_SHUFFLE(2, 1, 0, 3));
      s2 = _mm_shuffle_ps(s2, s2, _MM_SHUFFLE(2, 1, 0, 3));
      s3 = _mm_shuffle_ps(s3, s3, _MM_SHUFFLE(2, 1, 0, 3));
      s3 = _mm_move_ss(s3, s2);
      s2 = _mm_move_ss(s2, s1);
      s1 = _mm_move_ss(s1, _mm_add_ss(g, b));
      g = _mm_add_ss(g, kGapLeading);

      d1 = simd::blendv(_mm_castsi128_ps(c1), s1, kMin);
      d2 = simd::blendv(_mm_castsi128_ps(c2), s2, kMin);
      d3 = simd::blendv(_mm_castsi128_ps(c3), s3, kMin);
      m1 = _mm_max_ps(d1, _mm_add_ps(m1, kGap1));
      m2 = _mm_max_ps(d2, _mm_add_ps(m2, kGap1));
      m3 = _mm_max_ps(d3, _mm_add_ps(m3, kGap2));
    }

    return simd::extractv(m3, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 16) {
    auto nlo = _mm_unpacklo_epi8(nt, kZero);
    auto nhi = _mm_unpackhi_epi8(nt, kZero);
    auto n1 = _mm_unpacklo_epi16(nlo, kZero);
    auto n2 = _mm_unpackhi_epi16(nlo, kZero);
    auto n3 = _mm_unpacklo_epi16(nhi, kZero);
    auto n4 = _mm_unpackhi_epi16(nhi, kZero);
    auto d1 = kMin, d2 = kMin, d3 = kMin, d4 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin, m3 = kMin, m4 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = _mm_set1_epi32(toLower(ch));
      auto b = _mm_set1_ps(bonus);
      auto c1 = _mm_cmpeq_epi32(n1, r);
      auto c2 = _mm_cmpeq_epi32(n2, r);
      auto c3 = _mm_cmpeq_epi32(n3, r);
      auto c4 = _mm_cmpeq_epi32(n4, r);

      auto s1 = _mm_max_ps(_mm_add_ps(m1, b), _mm_add_ps(d1, kConsecutive));
      auto s2 = _mm_max_ps(_mm_add_ps(m2, b), _mm_add_ps(d2, kConsecutive));
      auto s3 = _mm_max_ps(_mm_add_ps(m3, b), _mm_add_ps(d3, kConsecutive));
      auto s4 = _mm_max_ps(_mm_add_ps(m4, b), _mm_add_ps(d4, kConsecutive));
      s1 = _mm_shuffle_ps(s1, s1, _MM_SHUFFLE(2, 1, 0, 3));
      s2 = _mm_shuffle_ps(s2, s2, _MM_SHUFFLE(2, 1, 0, 3));
      s3 = _mm_shuffle_ps(s3, s3, _MM_SHUFFLE(2, 1, 0, 3));
      s4 = _mm_shuffle_ps(s4, s4, _MM_SHUFFLE(2, 1, 0, 3));
      s4 = _mm_move_ss(s4, s3);
      s3 = _mm_move_ss(s3, s2);
      s2 = _mm_move_ss(s2, s1);
      s1 = _mm_move_ss(s1, _mm_add_ss(g, b));
      g = _mm_add_ss(g, kGapLeading);

      d1 = simd::blendv(_mm_castsi128_ps(c1), s1, kMin);
      d2 = simd::blendv(_mm_castsi128_ps(c2), s2, kMin);
      d3 = simd::blendv(_mm_castsi128_ps(c3), s3, kMin);
      d4 = simd::blendv(_mm_castsi128_ps(c4), s4, kMin);
      m1 = _mm_max_ps(d1, _mm_add_ps(m1, kGap1));
      m2 = _mm_max_ps(d2, _mm_add_ps(m2, kGap1));
      m3 = _mm_max_ps(d3, _mm_add_ps(m3, kGap1));
      m4 = _mm_max_ps(d4, _mm_add_ps(m4, kGap2));
    }

    return simd::extractv(m4, (needle.size() + 3) & 0b11);
  }
}

template Score scoreSSE<4>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreSSE<8>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreSSE<12>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreSSE<16>(const AlignedString& needle, std::string_view haystack) noexcept;

#endif // defined(FZX_SSE2)

#if defined(FZX_NEON)
template <size_t N>
Score scoreNeon(const AlignedString& needle, std::string_view haystack) noexcept
{
  static_assert(N == 4 || N == 8 || N == 12 || N == 16);

  DEBUG_ASSERT(needle.size() <= N);
  if (needle.empty() || haystack.size() > kMatchMaxLen || needle.size() > haystack.size()) {
    return kScoreMin;
  } else if (needle.size() == haystack.size()) {
    return kScoreMax;
  }

  const int haystackLen = static_cast<int>(haystack.size());

  const auto kMin = vmovq_n_f32(kScoreMin);
  const auto kGap1 [[maybe_unused]] = vmovq_n_f32(kScoreGapInner);
  const auto kGap2 = vld1q_f32(&kGapTable[3 - (needle.size() & 0b11)]);
  const auto kConsecutive = vmovq_n_f32(kScoreMatchConsecutive);
  const float kGapLeading = kScoreGapLeading;

  uint8_t lastCh = '/';
  float g = 0.f; // Leading gap score
  auto nt = simd::toLower(vld1q_u8(reinterpret_cast<const uint8_t*>(needle.data())));

  if constexpr (N == 4) {
    auto nl = vmovl_u8(vget_low_u8(nt));
    auto n = vmovl_u16(vget_low_u16(nl));
    auto d = kMin;
    auto m = kMin;

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = vmovq_n_u32(toLower(ch));
      auto b = vmovq_n_f32(bonus);
      auto c = vceqq_u32(n, r);
      auto s = vmaxq_f32(vaddq_f32(m, b), vaddq_f32(d, kConsecutive));
      s = vextq_f32(s, s, 3);
      s = vsetq_lane_f32(g + bonus, s, 0);
      g += kGapLeading;
      d = vbslq_f32(c, s, kMin);
      m = vmaxq_f32(d, vaddq_f32(m, kGap2));
    }

    return simd::extractv(m, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 8) {
    auto nl = vmovl_u8(vget_low_u8(nt));
    auto n1 = vmovl_u16(vget_low_u16(nl));
    auto n2 = vmovl_u16(vget_high_u16(nl));
    auto d1 = kMin, d2 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = vmovq_n_u32(toLower(ch));
      auto b = vmovq_n_f32(bonus);
      auto c1 = vceqq_u32(n1, r);
      auto c2 = vceqq_u32(n2, r);

      auto s1 = vmaxq_f32(vaddq_f32(m1, b), vaddq_f32(d1, kConsecutive));
      auto s2 = vmaxq_f32(vaddq_f32(m2, b), vaddq_f32(d2, kConsecutive));
      s1 = vextq_f32(s1, s1, 3);
      s2 = vextq_f32(s2, s2, 3);
      s2 = vsetq_lane_f32(vgetq_lane_f32(s1, 0), s2, 0);
      s1 = vsetq_lane_f32(g + bonus, s1, 0);
      g += kGapLeading;

      d1 = vbslq_f32(c1, s1, kMin);
      d2 = vbslq_f32(c2, s2, kMin);
      m1 = vmaxq_f32(d1, vaddq_f32(m1, kGap1));
      m2 = vmaxq_f32(d2, vaddq_f32(m2, kGap2));
    }

    return simd::extractv(m2, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 12) {
    auto nl = vmovl_u8(vget_low_u8(nt));
    auto nh = vmovl_u8(vget_high_u8(nt));
    auto n1 = vmovl_u16(vget_low_u16(nl));
    auto n2 = vmovl_u16(vget_high_u16(nl));
    auto n3 = vmovl_u16(vget_low_u16(nh));
    auto d1 = kMin, d2 = kMin, d3 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin, m3 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = vmovq_n_u32(toLower(ch));
      auto b = vmovq_n_f32(bonus);
      auto c1 = vceqq_u32(n1, r);
      auto c2 = vceqq_u32(n2, r);
      auto c3 = vceqq_u32(n3, r);

      auto s1 = vmaxq_f32(vaddq_f32(m1, b), vaddq_f32(d1, kConsecutive));
      auto s2 = vmaxq_f32(vaddq_f32(m2, b), vaddq_f32(d2, kConsecutive));
      auto s3 = vmaxq_f32(vaddq_f32(m3, b), vaddq_f32(d3, kConsecutive));
      s1 = vextq_f32(s1, s1, 3);
      s2 = vextq_f32(s2, s2, 3);
      s3 = vextq_f32(s3, s3, 3);
      s3 = vsetq_lane_f32(vgetq_lane_f32(s2, 0), s3, 0);
      s2 = vsetq_lane_f32(vgetq_lane_f32(s1, 0), s2, 0);
      s1 = vsetq_lane_f32(g + bonus, s1, 0);
      g += kGapLeading;

      d1 = vbslq_f32(c1, s1, kMin);
      d2 = vbslq_f32(c2, s2, kMin);
      d3 = vbslq_f32(c3, s3, kMin);
      m1 = vmaxq_f32(d1, vaddq_f32(m1, kGap1));
      m2 = vmaxq_f32(d2, vaddq_f32(m2, kGap1));
      m3 = vmaxq_f32(d3, vaddq_f32(m3, kGap2));
    }

    return simd::extractv(m3, (needle.size() + 3) & 0b11);
  } else if constexpr (N == 16) {
    auto nl = vmovl_u8(vget_low_u8(nt));
    auto nh = vmovl_u8(vget_high_u8(nt));
    auto n1 = vmovl_u16(vget_low_u16(nl));
    auto n2 = vmovl_u16(vget_high_u16(nl));
    auto n3 = vmovl_u16(vget_low_u16(nh));
    auto n4 = vmovl_u16(vget_high_u16(nh));
    auto d1 = kMin, d2 = kMin, d3 = kMin, d4 = kMin; // NOLINT(readability-isolate-declaration)
    auto m1 = kMin, m2 = kMin, m3 = kMin, m4 = kMin; // NOLINT(readability-isolate-declaration)

    for (int i = 0; i < haystackLen; ++i) {
      uint8_t ch = haystack[i];
      Score bonus = kBonusStates[kBonusIndex[ch]][lastCh];
      lastCh = ch;

      auto r = vmovq_n_u32(toLower(ch));
      auto b = vmovq_n_f32(bonus);
      auto c1 = vceqq_u32(n1, r);
      auto c2 = vceqq_u32(n2, r);
      auto c3 = vceqq_u32(n3, r);
      auto c4 = vceqq_u32(n4, r);

      auto s1 = vmaxq_f32(vaddq_f32(m1, b), vaddq_f32(d1, kConsecutive));
      auto s2 = vmaxq_f32(vaddq_f32(m2, b), vaddq_f32(d2, kConsecutive));
      auto s3 = vmaxq_f32(vaddq_f32(m3, b), vaddq_f32(d3, kConsecutive));
      auto s4 = vmaxq_f32(vaddq_f32(m4, b), vaddq_f32(d4, kConsecutive));
      s1 = vextq_f32(s1, s1, 3);
      s2 = vextq_f32(s2, s2, 3);
      s3 = vextq_f32(s3, s3, 3);
      s4 = vextq_f32(s4, s4, 3);
      s4 = vsetq_lane_f32(vgetq_lane_f32(s3, 0), s4, 0);
      s3 = vsetq_lane_f32(vgetq_lane_f32(s2, 0), s3, 0);
      s2 = vsetq_lane_f32(vgetq_lane_f32(s1, 0), s2, 0);
      s1 = vsetq_lane_f32(g + bonus, s1, 0);
      g += kGapLeading;

      d1 = vbslq_f32(c1, s1, kMin);
      d2 = vbslq_f32(c2, s2, kMin);
      d3 = vbslq_f32(c3, s3, kMin);
      d4 = vbslq_f32(c4, s4, kMin);
      m1 = vmaxq_f32(d1, vaddq_f32(m1, kGap1));
      m2 = vmaxq_f32(d2, vaddq_f32(m2, kGap1));
      m3 = vmaxq_f32(d3, vaddq_f32(m3, kGap1));
      m4 = vmaxq_f32(d4, vaddq_f32(m4, kGap2));
    }

    return simd::extractv(m4, (needle.size() + 3) & 0b11);
  }
}

template Score scoreNeon<4>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreNeon<8>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreNeon<12>(const AlignedString& needle, std::string_view haystack) noexcept;
template Score scoreNeon<16>(const AlignedString& needle, std::string_view haystack) noexcept;

#endif // defined(FZX_NEON)

Score matchPositions(const AlignedString& needle, std::string_view haystack, Positions* positions)
{
  if (needle.empty())
    return kScoreMin;

  MatchStruct match { needle, haystack };

  const int needleLen = match.mNeedleLen;
  const int haystackLen = match.mHaystackLen;

  if (haystackLen > kMatchMaxLen || needleLen > haystackLen) {
    // Unreasonably large candidate: return no score
    // If it is a valid match it will still be returned, it will
    // just be ranked below any reasonably sized candidates
    return kScoreMin;
  } else if (needleLen == haystackLen) {
    // Since this method can only be called with a haystack which
    // matches needle. If the lengths of the strings are equal the
    // strings themselves must also be equal (ignoring case).
    if (positions)
      for (int i = 0; i < needleLen; ++i)
        (*positions)[i] = i;
    return kScoreMax;
  }

  // D[][] Stores the best score for this position ending with a match.
  // M[][] Stores the best possible score at this position.
  std::vector<ScoreArray> d;
  std::vector<ScoreArray> m;
  d.resize(needleLen);
  m.resize(needleLen);

  Score* lastD = nullptr;
  Score* lastM = nullptr;
  Score* currD = nullptr;
  Score* currM = nullptr;

  for (int i = 0; i < needleLen; ++i) {
    currD = d[i].data();
    currM = m[i].data();

    match.matchRow(i, currD, currM, lastD, lastM);

    lastD = currD;
    lastM = currM;
  }

  // backtrace to find the positions of optimal matching
  if (positions) {
    bool matchRequired = false;
    for (int i = needleLen - 1, j = haystackLen - 1; i >= 0; --i) {
      for (; j >= 0; --j) {
        // There may be multiple paths which result in
        // the optimal weight.
        //
        // For simplicity, we will pick the first one
        // we encounter, the latest in the candidate
        // string.
        if (d[i][j] != kScoreMin && (matchRequired || d[i][j] == m[i][j])) {
          // If this score was determined using
          // kScoreMatchConsecutive, the
          // previous character MUST be a match
          matchRequired =
              i && j &&
              m[i][j] == d[i - 1][j - 1] + kScoreMatchConsecutive;
          (*positions)[i] = j--;
          break;
        }
      }
    }
  }

  return m[needleLen - 1][haystackLen - 1];
}

} // namespace fzx::fzy
