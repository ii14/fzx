// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cstdint>

namespace fzx {

struct Match
{
  uint32_t mIndex { 0 };
  float mScore { 0 };

  friend bool operator==(Match a, Match b) noexcept
  {
    return a.mIndex == b.mIndex && a.mScore == b.mScore;
  }

  friend bool operator<(Match a, Match b) noexcept
  {
    return a.mScore == b.mScore ? a.mIndex < b.mIndex : a.mScore > b.mScore;
  }

  friend bool operator>(Match a, Match b) noexcept
  {
    return a.mScore == b.mScore ? a.mIndex > b.mIndex : a.mScore < b.mScore;
  }

  friend bool operator!=(Match a, Match b) noexcept
  {
    return !(a == b);
  }

  friend bool operator<=(Match a, Match b) noexcept
  {
    return !(a > b);
  }

  friend bool operator>=(Match a, Match b) noexcept
  {
    return !(a < b);
  }
};

} // namespace fzx
