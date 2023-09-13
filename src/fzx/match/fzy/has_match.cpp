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

#include <cstdint>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/simd.hpp"
#include "fzx/util.hpp"

// TODO: avx2 and neon implementation
// TODO: find-first-set for MSVC

namespace fzx::fzy {

namespace {

// Portable implementation. Unfortunately it's really slow.
//
// The original fzy implementation of this function didn't seem to have any problems
// with performance. I think it's because it uses strpbrk, which is probably already
// optimized in glibc? The difference is that fzy uses null terminated strings, and
// we aren't, so it has to be optimized manually.
//
// Maybe it could be written in a way that can be autovectorized by the compiler?
[[maybe_unused]] bool hasMatchBasic(const AlignedString& needle, std::string_view haystack) noexcept
{
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
}

#if defined(FZX_SSE2) && FZX_HAS_BUILTIN(__builtin_ffs)
#define FZX_HAS_MATCH_SSE
[[maybe_unused]] bool hasMatchSSE(const AlignedString& needle, std::string_view haystack) noexcept
{
  // Technically needle and haystack never will be empty,
  // so maybe fix the tests and turn it into debug asserts
  if (needle.empty())
    return true;
  if (haystack.empty()) // Otherwise can lead to end < it
    return false;

  constexpr ptrdiff_t kWidth { 16 }; // Register width
  static_assert(isPow2(kWidth)); // -1 has to yield a mask for unaligned bytes
  constexpr uintptr_t kMisaligned { kWidth - 1 };

  const char* nit = needle.begin();
  const char* nend = needle.end();
  const char* it = haystack.begin();
  const char* end = haystack.end();

  // Loading memory from unaligned addresses is way slower.
  // Align the start and the end pointer to the width of the xmm register.
  auto offsetIt = reinterpret_cast<uintptr_t>(it) & kMisaligned;
  auto offsetEnd = reinterpret_cast<uintptr_t>(end) & kMisaligned;
  it -= offsetIt;
  if (offsetEnd == 0)
    offsetEnd = kWidth;
  end -= offsetEnd;
  DEBUG_ASSERT(end >= it);

  // Bit mask for the final chunk of memory.
  const uint32_t maskEnd = ~(uint32_t{0xFFFF} << offsetEnd);

  // Load the initial state into the registers.
  auto xmm0 = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(it)));
  auto xmm1 = _mm_set1_epi8(static_cast<char>(toLower(nit[0])));

  for (;;) {
    // Find positions in the chunk that match the current needle character.
    uint32_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));

    // Mask out of bounds memory out of the result.
    mask &= uint32_t{0xFFFF} << offsetIt;
    if (it == end)
      mask &= maskEnd;

    // Find the first occurence of the matched character in the haystack.
    // TODO: Mispredicted branch. Testing more characters at once might improve it.
    //       What might be worth to try is cramming in some speculative work and
    //       also checking nit[1]?
    if (int pos = __builtin_ffs(static_cast<int>(mask)); pos != 0) {
      // Found the character. If this was the last character in the needle, we have our match.
      if (++nit == nend)
        return true;

      // Load the next needle character.
      xmm1 = _mm_set1_epi8(static_cast<char>(toLower(nit[0])));

      // If it wasn't the last byte in the chunk, increment
      // the offset and match within the same chunk again.
      if (pos != kWidth) {
        offsetIt = pos;
        continue;
      }
      // If it was the last byte, load the next chunk of memory.
    }

    // No more data in the haystack, no match.
    if (it == end)
      return false;

    // Load the next chunk of memory.
    it += kWidth;
    xmm0 = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(it)));
    offsetIt = 0;
  }
}
#endif

} // namespace

bool hasMatch(const AlignedString& needle, std::string_view haystack) noexcept
{
#if defined(FZX_HAS_MATCH_SSE)
  return hasMatchSSE(needle, haystack);
#else
  return hasMatchBasic(needle, haystack);
#endif
}

} // namespace fzx::fzy
