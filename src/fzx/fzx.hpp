#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "fzx/item_list.hpp"
#include "fzx/tx_value.hpp"
#include "fzx/events.hpp"
#include "fzx/util.hpp"

namespace fzx {

struct Result
{
  std::string_view mLine;
  float mScore { 0 };
};

struct Match
{
  uint32_t mIndex { 0 };
  float mScore { 0.0 };

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

  friend bool operator!=(Match a, Match b) noexcept { return !(a == b); }
  friend bool operator<=(Match a, Match b) noexcept { return !(a > b); }
  friend bool operator>=(Match a, Match b) noexcept { return !(a < b); }
};

struct Results
{
  std::vector<Match> mResults;
  size_t mItemsTick { 0 };
  size_t mQueryTick { 0 };
};

struct Fzx
{
  Fzx();
  ~Fzx() noexcept;

  Fzx(const Fzx&) = delete;
  Fzx& operator=(const Fzx&) = delete;
  Fzx(Fzx&&) = delete;
  Fzx& operator=(Fzx&&) = delete;

  void start();
  void stop() noexcept;
  [[nodiscard]] int notifyFd() const noexcept { return mNotifyPipe[0]; }

  // TODO: add overflow checks in pushItem and scanFeed/scanEnd

  /// Push string to the list of items.
  void pushItem(std::string_view s) { mItems.push(s); }
  /// Commit added items (wake up the fzx thread).
  void commitItems() noexcept;
  [[nodiscard]] size_t itemsSize() const noexcept { return mItems.size(); }
  [[nodiscard]] std::string_view getItem(size_t i) const noexcept { return mItems.at(i); }

  /// Feed bytes into the line scanner.
  uint32_t scanFeed(std::string_view s);
  /// Finalize scanning - push any pending data that was left.
  bool scanEnd();

  void setQuery(std::string query) noexcept;

  bool loadResults() noexcept;
  [[nodiscard]] size_t resultsSize() const noexcept { return mResults.readBuffer().mResults.size(); }
  [[nodiscard]] Result getResult(size_t i) const noexcept;

  [[nodiscard]] bool processing() const noexcept
  {
    const auto& res = mResults.readBuffer();
    return res.mItemsTick != mItems.lastCommitSize() || res.mQueryTick != mQuery.writeTick();
  }

private:
  void run();

private:
  ItemList mItems;
  TxValue<std::string> mQuery;
  TxValue<Results> mResults;
  std::vector<char> mScanBuffer;

  int mNotifyPipe[2] { -1, -1 };
  std::atomic<bool> mNotifyActive { false };

  Events mEvents;
  std::thread mThread;
  bool mRunning { false };
};

} // namespace fzx
