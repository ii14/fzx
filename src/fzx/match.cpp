// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/match.hpp"

#include <cstdint>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"
#include "fzx/simd.hpp"
#include "fzx/strings.hpp"
#include "fzx/util.hpp"

namespace fzx {

namespace {

[[maybe_unused]] bool matchFuzzyNaive(const AlignedString& needle,
                                      std::string_view haystack) noexcept
{
  const char* it = haystack.data();
  const char* const end = it + haystack.size();
  for (char ch : needle) {
    char uch = static_cast<char>(toUpper(ch));
    while (it != end && *it != ch && *it != uch)
      ++it;
    if (it == end)
      return false;
    ++it;
  }
  return true;
}

#if defined(FZX_SSE2)
[[maybe_unused]] bool matchFuzzySSE(const AlignedString& needle, std::string_view haystack) noexcept
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

  const char* nit = needle.data();
  const char* nend = nit + needle.size();
  const char* it = haystack.data();
  const char* end = it + haystack.size();

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
  const uint32_t maskEnd = ~(uint32_t { 0xFFFF } << offsetEnd);

  // Load the initial state into the registers.
  auto xmm0 = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(it)));
  auto xmm1 = _mm_set1_epi8(static_cast<char>(toLower(nit[0])));

  for (;;) {
    // Find positions in the chunk that match the current needle character.
    uint32_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));

    // Mask out of bounds memory out of the result.
    mask &= uint32_t { 0xFFFF } << offsetIt;
    if (it == end)
      mask &= maskEnd;

    // Find the first occurence of the matched character in the haystack.
    // TODO: Mispredicted branch. Testing more characters at once might improve it.
    //       What might be worth to try is cramming in some speculative work and
    //       also checking nit[1]?
    if (int pos = ffs32(mask); pos != 0) {
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

bool matchFuzzy(const AlignedString& needle, std::string_view haystack) noexcept
{
#if defined(FZX_SSE2)
  return matchFuzzySSE(needle, haystack);
#else
  return matchFuzzyNaive(needle, haystack);
#endif
}

bool matchBegin(const AlignedString& needle, std::string_view haystack) noexcept
{
  // TODO: sse
  if (needle.size() > haystack.size())
    return false;
  const char* n = needle.data();
  const char* nend = n + needle.size();
  const char* h = haystack.data();
  while (n != nend) {
    if (toLower(*n) != toLower(*h))
      return false;
    ++n;
    ++h;
  }
  return true;
}

bool matchEnd(const AlignedString& needle, std::string_view haystack) noexcept
{
  // TODO: sse
  if (needle.size() > haystack.size())
    return false;
  const char* n = needle.data();
  const char* nend = n + needle.size();
  const char* h = haystack.data() + haystack.size() - needle.size();
  while (n != nend) {
    if (toLower(*n) != toLower(*h))
      return false;
    ++n;
    ++h;
  }
  return true;
}

bool matchExact(const AlignedString& needle, std::string_view haystack) noexcept
{
  // TODO: sse
  if (needle.size() != haystack.size())
    return false;
  const char* n = needle.data();
  const char* nend = n + needle.size();
  const char* h = haystack.data();
  while (n != nend) {
    if (toLower(*n) != toLower(*h))
      return false;
    ++n;
    ++h;
  }
  return true;
}

} // namespace fzx
