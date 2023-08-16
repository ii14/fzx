#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "fzx/eventfd.hpp"
#include "fzx/items.hpp"
#include "fzx/line_scanner.hpp"
#include "fzx/match.hpp"
#include "fzx/worker.hpp"

namespace fzx {

// TODO: refactor

struct Result
{
  std::string_view mLine;
  uint32_t mIndex { 0 };
  float mScore { 0 };
};

/// Atomic counter for adding queue functionality on top of fzx::Items
struct ItemQueue
{
  // This atomic counter is not synchronizing anything, hence the relaxed atomics.

  /// Reserve N items and return the start index of reserved range.
  [[nodiscard]] size_t take(size_t n) noexcept
  {
    return mIndex.fetch_add(n, std::memory_order_relaxed);
  }

  /// Get the current value, for reporting the progress.
  [[nodiscard]] size_t get() const noexcept
  {
    return mIndex.load(std::memory_order_relaxed);
  }

private:
  alignas(kCacheLine) std::atomic<size_t> mIndex { 0 };
};

struct Job
{
  /// Items to process. The size is monotonically increasing.
  Items mItems;
  /// Active query.
  std::shared_ptr<std::string> mQuery;
  /// Shared atomic counter for reserving the items for processing.
  std::shared_ptr<ItemQueue> mQueue;
  /// Monotonically increasing timestamp identifying the active query.
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

  void setThreads(unsigned threads) noexcept;

  void start();
  void stop();
  [[nodiscard]] int notifyFd() const noexcept { return mEventFd.fd(); }

  /// Push string to the list of items.
  void pushItem(std::string_view s) { mItems.push(s); }
  [[nodiscard]] size_t itemsSize() const noexcept { return mItems.size(); }
  [[nodiscard]] std::string_view getItem(size_t i) const noexcept { return mItems.at(i); }

  /// Feed bytes into the line scanner.
  uint32_t scanFeed(std::string_view s);
  /// Finalize scanning - push any pending data that was left.
  bool scanEnd();

  /// Set query
  void setQuery(std::string query);

  /// Publish changes and wake up worker threads.
  void commit();

  /// Load results accessed with resultsSize, getResult, query, processing.
  bool loadResults() noexcept;
  [[nodiscard]] size_t resultsSize() const noexcept;
  [[nodiscard]] Result getResult(size_t i) const noexcept;
  /// Get the original query for the current results.
  /// Might not be in sync with what was just set with setQuery.
  [[nodiscard]] std::string_view query() const;
  /// Check if the current results are up-to-date.
  [[nodiscard]] bool processing() const noexcept;
  /// Get estimated progress, value between 0.0 and 1.0.
  /// Value changes independently of loadResults.
  [[nodiscard]] double progress() const noexcept;

private:
  [[nodiscard]] const Results* getResults() const noexcept
  {
    if (const Worker* master = masterWorker(); master != nullptr)
      return &master->mOutput.readBuffer();
    return nullptr;
  }

  [[nodiscard]] Worker* masterWorker() noexcept
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

  [[nodiscard]] const Worker* masterWorker() const noexcept
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

  /// Load current job, for worker threads
  void loadJob(Job& job) const
  {
    std::shared_lock lock { mJobMutex };
    job = mJob;
  }

private:
  Items mItems;
  std::shared_ptr<std::string> mQuery;
  std::shared_ptr<ItemQueue> mQueue;

  LineScanner mLineScanner;
  /// Worker threads. This vector is shared with workers, so after
  /// starting and before joining threads, it cannot be modified.
  std::vector<std::unique_ptr<Worker>> mWorkers {};
  /// Notifies external event loop. Used by worker threads, so it
  /// cannot be closed before joining threads.
  EventFd mEventFd;

  /// Worker count.
  unsigned mThreads { 1 };
  /// Keep track of whether the threads are running, to
  /// ensure correct usage of start and stop methods.
  bool mRunning { false };

  /// Main thread needs a write lock when modifying, can read without holding any lock.
  /// Worker threads need a read lock and are not allowed to modify it it any way.
  /// They should use loadJob method.
  Job mJob;
  alignas(kCacheLine) mutable std::shared_mutex mJobMutex;

  friend struct Worker;
};

} // namespace fzx
