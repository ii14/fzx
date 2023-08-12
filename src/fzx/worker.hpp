#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "fzx/events.hpp"
#include "fzx/match.hpp"
#include "fzx/tx.hpp"

namespace fzx {

struct Fzx;

struct Results
{
  /// Matched items.
  std::vector<Match> mItems;
  /// Original query.
  /// By the time the results are sent back, the active query could've changed, so
  /// it's necessary to pass it back to make sure matched positions are calculated
  /// using the correct query.
  std::shared_ptr<std::string> mQuery;
  /// Timestamp identifying items. Last known items size.
  size_t mItemsTick { 0 };
  /// Timestamp identifying the query.
  size_t mQueryTick { 0 };

  [[nodiscard]] bool newerThan(const Results& b) const noexcept
  {
    return mItemsTick > b.mItemsTick || mQueryTick > b.mQueryTick;
  }

  [[nodiscard]] bool sameTick(const Results& b) const noexcept
  {
    return mItemsTick == b.mItemsTick && mQueryTick == b.mQueryTick;
  }
};

struct Worker
{
  enum Event : uint32_t {
    kNone = 0,
    kStop = 1U << 0,
    kJob = 1U << 1,
    kMerge = 1U << 2,
  };

  std::thread mThread;
  Tx<Results> mOutput;
  Events mEvents;
  Fzx* mPool { nullptr };
  uint8_t mIndex { 0 };

  void run() const;
};

} // namespace fzx
