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

#include <cstddef>
#include <string_view>
#include <array>

namespace fzx::fzy {

using Score = double;

static constexpr auto kMatchMaxLen = 1024;

/// NOTE: When compiled with SSE2 or AVX2 enabled, it reads memory out of bounds of string_view.
/// Needle and haystack need fzx::kOveralloc bytes of valid memory after the end of the string.
bool hasMatch(std::string_view needle, std::string_view haystack) noexcept;

/// NOTE: When compiled with SSE2 or AVX2 enabled, it reads memory out of bounds of string_view.
/// Needle and haystack need fzx::kOveralloc bytes of valid memory after the end of the string.
Score match(std::string_view needle, std::string_view haystack);

using Positions = std::array<size_t, kMatchMaxLen>;
Score matchPositions(std::string_view needle, std::string_view haystack, Positions* positions);

} // namespace fzx::fzy
