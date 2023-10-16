// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <string_view>

#include "fzx/aligned_string.hpp"

namespace fzx {

bool matchFuzzyNaive(std::string_view needle, std::string_view haystack) noexcept;
#if defined(FZX_SSE2)
bool matchFuzzySSE(const AlignedString& needle, std::string_view haystack) noexcept;
#endif
bool matchFuzzy(const AlignedString& needle, std::string_view haystack) noexcept;

// unused
bool matchBegin(const AlignedString& needle, std::string_view haystack) noexcept;
bool matchEnd(const AlignedString& needle, std::string_view haystack) noexcept;
bool matchExact(const AlignedString& needle, std::string_view haystack) noexcept;
// bool matchSubstr(const AlignedString& needle, std::string_view haystack) noexcept; // TODO

} // namespace fzx
