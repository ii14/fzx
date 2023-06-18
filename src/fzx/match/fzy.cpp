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
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <utility>

namespace fzx {

static constexpr auto kBonusStates = []{
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

static constexpr auto kBonusIndex = []{
  std::array<uint8_t, 256> r {};
  for (char i = 'A'; i <= 'Z'; ++i)
    r[i] = 2;
  for (char i = 'a'; i <= 'z'; ++i)
    r[i] = 1;
  for (char i = '0'; i <= '9'; ++i)
    r[i] = 1;
  return r;
}();

static constexpr Score computeBonus(uint8_t lastCh, uint8_t ch)
{
  return kBonusStates[kBonusIndex[ch]][lastCh];
}

bool hasMatch(std::string_view needle, std::string_view haystack)
{
  const char* it = haystack.begin();
  for (char ch : needle) {
    char uch = char(toupper(ch));
    while (it != haystack.end() && (*it != ch && *it != uch))
      ++it;
    if (it == haystack.end())
      return false;
    ++it;
  }
  return true;
}

static void precomputeBonus(std::string_view haystack, Score* matchBonus)
{
  // Which positions are beginning of words
  char lastCh = '/';
  for (size_t i = 0; i < haystack.size(); ++i) {
    char ch = haystack[i];
    matchBonus[i] = computeBonus(lastCh, ch);
    lastCh = ch;
  }
}

using ScoreArray = std::array<Score, kMatchMaxLen>;

struct MatchStruct
{
  int mNeedleLen {};
  int mHaystackLen {};

  char mLowerNeedle[kMatchMaxLen] {};
  char mLowerHaystack[kMatchMaxLen] {};

  Score mMatchBonus[kMatchMaxLen] {};

  MatchStruct(std::string_view needle, std::string_view haystack);

  inline void matchRow(
    int row,
    ScoreArray& currD,
    ScoreArray& currM,
    const ScoreArray& lastD,
    const ScoreArray& lastM);
};

MatchStruct::MatchStruct(std::string_view needle, std::string_view haystack)
  : mNeedleLen(int(needle.size()))
  , mHaystackLen(int(haystack.size()))
{
  if (mHaystackLen > kMatchMaxLen || mNeedleLen > mHaystackLen)
    return;

  for (int i = 0; i < mNeedleLen; ++i)
    mLowerNeedle[i] = char(tolower(needle[i]));
  for (int i = 0; i < mHaystackLen; ++i)
    mLowerHaystack[i] = char(tolower(haystack[i]));

  precomputeBonus(haystack, mMatchBonus);
}

inline void MatchStruct::matchRow(
    int row,
    ScoreArray& currD,
    ScoreArray& currM,
    const ScoreArray& lastD,
    const ScoreArray& lastM)
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

  ScoreArray* lastD = &D[0];
  ScoreArray* lastM = &M[0];
  ScoreArray* currD = &D[1];
  ScoreArray* currM = &M[1];

  for (int i = 0; i < match.mNeedleLen; ++i) {
    match.matchRow(i, *currD, *currM, *lastD, *lastM);
    std::swap(currD, lastD);
    std::swap(currM, lastM);
  }

  return (*lastM)[match.mHaystackLen - 1];
}

// Score matchPositions(const char* needle, const char* haystack, size_t* positions)
// {
//   if (!*needle)
//     return kScoreMin;

//   MatchStruct match { needle, haystack };

//   const int n = match.mNeedleLen;
//   const int m = match.mHaystackLen;

//   if (m > kMatchMaxLen || n > m) {
//     // Unreasonably large candidate: return no score
//     // If it is a valid match it will still be returned, it will
//     // just be ranked below any reasonably sized candidates
//     return kScoreMin;
//   } else if (n == m) {
//     // Since this method can only be called with a haystack which
//     // matches needle. If the lengths of the strings are equal the
//     // strings themselves must also be equal (ignoring case).
//     if (positions)
//       for (int i = 0; i < n; ++i)
//         positions[i] = i;
//     return kScoreMax;
//   }

//   using ScoreTable = Score (*)[kMatchMaxLen];
//   // D[][] Stores the best score for this position ending with a match.
//   // M[][] Stores the best possible score at this position.
//   auto* D = reinterpret_cast<ScoreTable>(new Score[kMatchMaxLen * n]);
//   auto* M = reinterpret_cast<ScoreTable>(new Score[kMatchMaxLen * n]);

//   Score *lastD = nullptr;
//   Score *lastM = nullptr;
//   Score *currD = nullptr;
//   Score *currM = nullptr;

//   for (int i = 0; i < n; ++i) {
//     currD = &D[i][0];
//     currM = &M[i][0];

//     match.matchRow(i, currD, currM, lastD, lastM);

//     lastD = currD;
//     lastM = currM;
//   }

//   // backtrace to find the positions of optimal matching
//   if (positions) {
//     bool matchRequired = false;
//     for (int i = n - 1, j = m - 1; i >= 0; --i) {
//       for (; j >= 0; --j) {
//         // There may be multiple paths which result in
//         // the optimal weight.
//         //
//         // For simplicity, we will pick the first one
//         // we encounter, the latest in the candidate
//         // string.
//         if (D[i][j] != kScoreMin && (matchRequired || D[i][j] == M[i][j])) {
//           // If this score was determined using
//           // kScoreMatchConsecutive, the
//           // previous character MUST be a match
//           matchRequired =
//               i && j &&
//               M[i][j] == D[i - 1][j - 1] + kScoreMatchConsecutive;
//           positions[i] = j--;
//           break;
//         }
//       }
//     }
//   }

//   Score result = M[n - 1][m - 1];

//   delete[] M;
//   delete[] D;

//   return result;
// }

} // namespace fzx
