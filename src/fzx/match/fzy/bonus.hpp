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

#pragma once

#include <array>

#include "fzx/match/fzy/config.hpp"

namespace fzx::fzy {

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

} // namespace fzx::fzy
