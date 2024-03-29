// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <string_view>

#include "fzx/aligned_string.hpp"

namespace fzx {

/// Precondition: haystack is aligned to 16 bytes and is padded to 16 bytes with zeros
bool matchFuzzy(const AlignedString& needle, std::string_view haystack) noexcept;

bool matchBegin(const AlignedString& needle, std::string_view haystack) noexcept;

bool matchEnd(const AlignedString& needle, std::string_view haystack) noexcept;

bool matchExact(const AlignedString& needle, std::string_view haystack) noexcept;

bool matchSubstr(const AlignedString& needle, std::string_view haystack) noexcept;
int matchSubstrIndex(const AlignedString& needle, std::string_view haystack) noexcept;

} // namespace fzx
