// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include "fzx/score.hpp"
#include "fzx/util.hpp"

namespace fzx {

/// Matched item. Contains the item index and calculated score.
class MatchedItem
{
  // Item index is encoded in the low 32 bits, and score is encoded in the high 32 bits.
  // This allows the less-than logic below to be calculated with a single int64 comparison.
  //
  //     (a < b) == (a.score != b.score ? a.score > b.score : a.index < b.index)
  int64_t mValue { 0 };

  static constexpr auto kMin = std::numeric_limits<int32_t>::min();
  static constexpr auto kMax = std::numeric_limits<int32_t>::max();
  static constexpr auto kInf = std::numeric_limits<float>::infinity();

public:
  MatchedItem() noexcept = default;

  [[nodiscard]] int64_t value() const { return mValue; }

  MatchedItem(uint32_t index, float score) noexcept
  {
    int32_t hi; // NOLINT(cppcoreguidelines-init-variables)
    if (!std::isinf(score)) {
      DEBUG_ASSERT(!std::isnan(score));
      // No loss of precision.
      //
      // Right now there is a limit on the haystack length, and the max possible
      // score is 1024 * 200 = 204800. With the current scoring the limit could
      // be raised to UINT16_MAX just fine. If necessary, it of course could be
      // extended to the full range of int32 in the future.
      DEBUG_ASSERT(score >= -16777216.F && score <= 16777216.F);
      // Negate the score value, to prefer higher scores in the less-than operator.
      hi = -static_cast<int32_t>(score);
    } else {
      hi = std::signbit(score) ? kMax : kMin;
    }
    mValue = static_cast<int64_t>((uint64_t{static_cast<uint32_t>(hi)} << 32) | uint64_t{index});
  }

  [[nodiscard]] uint32_t index() const noexcept
  {
    return mValue & 0xFFFFFFFFU;
  }

  [[nodiscard]] float score() const noexcept
  {
    auto v = static_cast<int32_t>(static_cast<uint64_t>(mValue) >> 32);
    if (v == kMax) {
      return -kInf;
    } else if (v == kMin) {
      return kInf;
    } else {
      return -static_cast<float>(v);
    }
  }

  friend bool operator==(MatchedItem a, MatchedItem b) noexcept { return a.mValue == b.mValue; }
  friend bool operator!=(MatchedItem a, MatchedItem b) noexcept { return a.mValue != b.mValue; }
  friend bool operator<(MatchedItem a, MatchedItem b) noexcept { return a.mValue < b.mValue; }
  friend bool operator>(MatchedItem a, MatchedItem b) noexcept { return a.mValue > b.mValue; }
  friend bool operator<=(MatchedItem a, MatchedItem b) noexcept { return a.mValue <= b.mValue; }
  friend bool operator>=(MatchedItem a, MatchedItem b) noexcept { return a.mValue >= b.mValue; }
};

} // namespace fzx
