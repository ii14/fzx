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

#include "fzx/match/fzy/fzy.hpp"

#include <limits>

namespace fzx::fzy {

// Scores have been multiplied by 200 to operate on whole numbers, which simplifies things.
// Multiply the result score by kScoreMultiplier to get back a more readable value.
//
// Be careful with changing the values. The maximum and minimum score value times kMatchMaxLen
// should fit in [-16777216, 16777216] range. See the comments on fzx::Match class.

constexpr Score kScoreMultiplier = 0.005;

constexpr Score kScoreGapLeading = -1;
constexpr Score kScoreGapTrailing = -1;
constexpr Score kScoreGapInner = -2;
constexpr Score kScoreMatchConsecutive = 200;
constexpr Score kScoreMatchSlash = 180;
constexpr Score kScoreMatchWord = 160;
constexpr Score kScoreMatchCapital = 140;
constexpr Score kScoreMatchDot = 120;

constexpr Score kScoreMax = std::numeric_limits<Score>::infinity();
constexpr Score kScoreMin = -std::numeric_limits<Score>::infinity();

} // namespace fzx::fzy
