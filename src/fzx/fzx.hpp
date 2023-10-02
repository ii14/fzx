// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "fzx/aligned_string.hpp"
#include "fzx/item_queue.hpp"
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

struct Job
{
  /// Items to process. The size is monotonically increasing.
  Items mItems;
  /// Active query.
  std::shared_ptr<AlignedString> mQuery;
  /// Shared atomic counter for reserving the items for processing.
  std::shared_ptr<ItemQueue> mQueue;
  /// Monotonically increasing timestamp identifying the active query.
  size_t mQueryTick { 0 };
};

using Callback = void (*)(void* userData);

struct Fzx
{
  Fzx();
  ~Fzx() noexcept;

  Fzx(const Fzx&) = delete;
  Fzx& operator=(const Fzx&) = delete;
  Fzx(Fzx&&) = delete;
  Fzx& operator=(Fzx&&) = delete;

  /// Set callback function. Called when results for the last query are available.
  /// Callback can be called from different threads, it has to be thread-safe, even
  /// in regards to itself.
  void setCallback(Callback callback, void* userData = nullptr) noexcept;
  void setThreads(unsigned threads) noexcept;

  void start();
  void stop();

  /// Push string to the list of items.
  void pushItem(std::string_view s) { mItems.push(s); }
  [[nodiscard]] size_t itemsSize() const noexcept { return mItems.size(); }
  [[nodiscard]] std::string_view getItem(size_t i) const noexcept { return mItems.at(i); }

  /// Set query
  void setQuery(std::string_view query);

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

  /// Check if results are synchronized.
  /// Only useful for testing and benchmarking, prefer using Fzx::processing.
  bool synchronized() const noexcept;

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
  std::shared_ptr<AlignedString> mQuery;
  std::shared_ptr<ItemQueue> mQueue;

  LineScanner mLineScanner;
  /// Worker threads. This vector is shared with workers, so after
  /// starting and before joining threads, it cannot be modified.
  std::vector<std::unique_ptr<Worker>> mWorkers {};

  Callback mCallback { nullptr };
  void* mUserData { nullptr };

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
