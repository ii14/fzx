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

[[maybe_unused]] int matchFuzzyNaive(const AlignedString& needle,
                                     std::string_view haystack) noexcept
{
  const char* it = haystack.data();
  const char* const end = it + haystack.size();
  for (char ch : needle) {
    char uch = static_cast<char>(toUpper(ch));
    while (it != end && *it != ch && *it != uch)
      ++it;
    if (it == end)
      return -1;
    ++it;
  }
  // TODO: SIMD version returns the index of the first matched character, that then can
  // be used to skip some iterations in the scoring function. Non-SIMD scoring doesn't
  // seem to benefit from this optimization, so I don't bother doing the same here.
  // It's possible I missed something though, so it might be worth it to go over this
  // more carefully again and understand what is happening.
  return 0;
}

#if defined(FZX_SSE2)
// TODO: port to neon
[[maybe_unused]] int matchFuzzySSE(const AlignedString& needle, std::string_view haystack) noexcept
{
  // TODO: haystack is padded with zeros. if needle includes zeros, it can match those.
  //       i think it's best to just ban zeros from queries.
  constexpr auto kAlign = 16;

  const char* ndIt = needle.data();
  const char* const ndEnd = ndIt + needle.size();
  if (ndIt == ndEnd)
    return 0;

  const char* hsIt = haystack.data();
  DEBUG_ASSERT(isAligned<kAlign>(hsIt));
  const char* const hsEnd = hsIt + roundUp<kAlign>(haystack.size());
  if (hsIt == hsEnd)
    return -1;

  // Initial state of registers
  auto hs = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(hsIt)));
  auto nd = _mm_set1_epi8(static_cast<char>(toLower(*ndIt)));
  uint32_t pos = 0; // Current position in a 16-byte chunk
  int res = -1;

  for (;;) {
    uint32_t mask = _mm_movemask_epi8(_mm_cmpeq_epi8(hs, nd));
    mask &= uint32_t { 0xFFFF } << pos; // Mask out past positions

    if (mask != 0) { // Found a character
      if (++ndIt == ndEnd) { // No characters left in the needle...
        if (res != -1)
          return res;
        return static_cast<int>(hsIt - haystack.data()) + ffs32(mask) - 1; // ...success
      }
      nd = _mm_set1_epi8(static_cast<char>(toLower(*ndIt))); // Load the next needle character

      // We *know* there is a bit here somewhere, find its position
      pos = ffs32(mask); // Last (16th) position masked out
      if (res == -1)
        res = static_cast<int>(hsIt - haystack.data()) + static_cast<int>(pos) - 1;
      if (pos != 16) { // It wasn't the last character in this chunk...
        pos = pos & 15;
        continue; // ...try matching this chunk again...
      }
      // ...otherwise load the next 16 bytes from the haystack
    }

    hsIt += kAlign;
    if (hsIt == hsEnd) // Nothing left in the haystack, characters still left in the needle...
      return -1; // ...no match
    hs = simd::toLower(_mm_load_si128(reinterpret_cast<const __m128i*>(hsIt))); // Next 16 bytes
    pos = 0; // Reset current position in the chunk
  }
}
#endif

} // namespace

int matchFuzzy(const AlignedString& needle, std::string_view haystack) noexcept
{
#if defined(FZX_SSE2)
  return matchFuzzySSE(needle, haystack);
#else
  return matchFuzzyNaive(needle, haystack);
#endif
}

bool matchBegin(const AlignedString& needle, std::string_view haystack) noexcept
{
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

bool matchSubstr(const AlignedString& needle, std::string_view haystack) noexcept
{
  // TODO: performance is terrible here
  if (needle.empty())
    return true;
  if (needle.size() > haystack.size())
    return false;

  const char* n = needle.data();
  const char* nend = n + needle.size();
  const char* h = haystack.data();
  const char* hend = h + haystack.size() - needle.size() + 1;

  while (h != hend) {
    auto cmp = [&]() -> bool {
      for (const char *np = n, *hp = h; np != nend; ++np, ++hp)
        if (toLower(*np) != toLower(*hp))
          return false;
      return true;
    };

    if (cmp())
      return true;
    ++h;
  }

  return false;
}

int matchSubstrIndex(const AlignedString& needle, std::string_view haystack) noexcept
{
  if (needle.empty())
    return 0;
  if (needle.size() > haystack.size())
    return -1;

  const int nEnd = static_cast<int>(needle.size());
  const int hEnd = static_cast<int>(haystack.size() - needle.size()) + 1;

  for (int i = 0; i != hEnd; ++i) {
    auto cmp = [&]() -> bool {
      for (int n = 0, h = i; n != nEnd; ++n, ++h)
        if (toLower(needle[n]) != toLower(haystack[h]))
          return false;
      return true;
    };

    if (cmp())
      return i;
  }

  return -1;
}

} // namespace fzx
