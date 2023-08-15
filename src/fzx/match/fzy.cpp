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

bool hasMatch(std::string_view needle, std::string_view haystack) noexcept
{
  // TODO: avx, neon
#if defined(FZX_SSE2) && FZX_HAS_BUILTIN(__builtin_ffs)
  // Interestingly, the original fzy doesn't seem to have any problems with performance here.
  // If I had to guess, it's probably because it uses strpbrk, which is probably optimized
  // already in glibc? We're not using null terminated strings though.
  static_assert(fzx::kOveralloc >= 16);
  const char* np = needle.begin();
  const char* hp = haystack.begin();
  const char* const nEnd = needle.end();
  const char* const hEnd = haystack.end();
  for (;;) {
    if (np == nEnd)
      return true;
    if (hp >= hEnd)
      return false;
    auto n = _mm_set1_epi8(static_cast<char>(toLower(*np)));
    auto h = simd::load128i(hp);
    h = simd::toLower(h);
    uint16_t r = _mm_movemask_epi8(_mm_cmpeq_epi8(h, n));
    ptrdiff_t d = hEnd - hp; // How many bytes are left in the haystack until the end
    r &= d < 16 ? 0xFFFFU >> (16 - d) : 0xFFFFU; // Mask out positions out of bounds
    hp += r ? __builtin_ffs(r) : 16; // Skip to after the first matched character
    np += static_cast<bool>(r); // Next needle character if we had a match
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

  MatchStruct(std::string_view needle, std::string_view haystack);

  inline void matchRow(
    int row,
    Score* RESTRICT currD,
    Score* RESTRICT currM,
    const Score* RESTRICT lastD,
    const Score* RESTRICT lastM);
};

// "initialize all your variables" they said.
// and now memset takes up 20% of the runtime of your program.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
MatchStruct::MatchStruct(std::string_view needle, std::string_view haystack)
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
    const Score* RESTRICT lastM)
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

Score match(std::string_view needle, std::string_view haystack)
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
