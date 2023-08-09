#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "fzx/eventfd.hpp"
#include "fzx/events.hpp"
#include "fzx/line_scanner.hpp"
#include "fzx/match.hpp"
#include "fzx/tx.hpp"
#include "fzx/worker_pool.hpp"

namespace fzx {

struct Result
{
  std::string_view mLine;
  uint32_t mIndex { 0 };
  float mScore { 0 };
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
  [[nodiscard]] int notifyFd() const noexcept { return mPool.mEventFd.fd(); }

  /// Push string to the list of items.
  void pushItem(std::string_view s) { mJob.mItems.push(s); }
  /// Commit added items (wake up the fzx thread).
  void commitItems() noexcept;
  [[nodiscard]] size_t itemsSize() const noexcept { return mJob.mItems.size(); }
  [[nodiscard]] std::string_view getItem(size_t i) const noexcept { return mJob.mItems.at(i); }

  /// Feed bytes into the line scanner.
  uint32_t scanFeed(std::string_view s);
  /// Finalize scanning - push any pending data that was left.
  bool scanEnd();

  /// Set query
  void setQuery(std::string query) noexcept;
  /// Get active query (might not be in sync with what was just set with setQuery)
  [[nodiscard]] std::string_view query() const;

  bool loadResults() noexcept;
  [[nodiscard]] size_t resultsSize() const noexcept;
  [[nodiscard]] Result getResult(size_t i) const noexcept;

  [[nodiscard]] bool processing() const noexcept;

private:
  Job mJob;
  WorkerPool mPool;
  LineScanner mLineScanner;
  bool mRunning { false };
};

} // namespace fzx
