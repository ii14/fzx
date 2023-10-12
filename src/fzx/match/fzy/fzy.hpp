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

#include <cstddef>
#include <string_view>
#include <array>

#include "fzx/aligned_string.hpp"

namespace fzx::fzy {

using Score = float;

static constexpr auto kMatchMaxLen = 1024;

Score score(const AlignedString& needle, std::string_view haystack) noexcept;
Score score1(const AlignedString& needle, std::string_view haystack) noexcept;

#if defined(FZX_SSE2)
template <size_t N> Score scoreSSE(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreSSE<4>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreSSE<8>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreSSE<12>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreSSE<16>(const AlignedString& needle, std::string_view haystack) noexcept;
#endif // defined(FZX_SSE2)

#if defined(FZX_NEON)
template <size_t N> Score scoreNeon(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreNeon<4>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreNeon<8>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreNeon<12>(const AlignedString& needle, std::string_view haystack) noexcept;
extern template Score scoreNeon<16>(const AlignedString& needle, std::string_view haystack) noexcept;
#endif // defined(FZX_NEON)

using Positions = std::array<size_t, kMatchMaxLen>;
Score matchPositions(const AlignedString& needle, std::string_view haystack, Positions* positions);

} // namespace fzx::fzy
