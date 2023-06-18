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
#include "fzx/util.hpp"

namespace fzx {

struct Result
{
  std::string_view mLine;
  double mScore { 0 };
};

struct Match
{
  size_t mIndex { 0 };
  double mScore { 0.0 };

  friend bool operator==(const Match& a, const Match& b) noexcept
  {
    return a.mIndex == b.mIndex && a.mScore == b.mScore;
  }

  friend bool operator<(const Match& a, const Match& b) noexcept
  {
    return a.mScore == b.mScore ? a.mIndex < b.mIndex : a.mScore > b.mScore;
  }

  friend bool operator>(const Match& a, const Match& b) noexcept
  {
    return a.mScore == b.mScore ? a.mIndex > b.mIndex : a.mScore < b.mScore;
  }

  friend bool operator!=(const Match& a, const Match& b) noexcept { return !(a == b); }
  friend bool operator<=(const Match& a, const Match& b) noexcept { return !(a > b); }
  friend bool operator>=(const Match& a, const Match& b) noexcept { return !(a < b); }
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

  void pushItem(std::string_view s) { mItems.push(s); }
  void commitItems() noexcept;
  [[nodiscard]] size_t itemsSize() const noexcept { return mItems.size(); }
  [[nodiscard]] std::string_view getItem(size_t i) const noexcept { return mItems.at(i); }

  void setQuery(std::string query) noexcept;

  bool loadResults() noexcept;
  [[nodiscard]] size_t resultsSize() const noexcept { return mResults.readBuffer().size(); }
  [[nodiscard]] Result getResult(size_t i) const noexcept;

private:
  void run();

  ItemList mItems;
  TxValue<std::string> mQuery;
  TxValue<std::vector<Match>> mResults;

  int mNotifyPipe[2] { -1, -1 };
  std::atomic<bool> mNotifyActive { false };

  mutable std::mutex mMutex;
  mutable std::condition_variable mCv;
  std::thread mThread;
  std::atomic<uint32_t> mUpdate { false };
  bool mRunning { false };
};

} // namespace fzx
