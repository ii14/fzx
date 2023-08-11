#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "fzx/eventfd.hpp"
#include "fzx/events.hpp"
#include "fzx/items.hpp"
#include "fzx/lr.hpp"
#include "fzx/match.hpp"
#include "fzx/tx.hpp"

namespace fzx {

struct Output
{
  std::vector<Match> mItems;
  std::shared_ptr<std::string> mQuery;
  size_t mItemsTick { 0 };
  size_t mQueryTick { 0 };

  [[nodiscard]] bool newerThan(const Output& b) const noexcept
  {
    return mItemsTick > b.mItemsTick || mQueryTick > b.mQueryTick;
  }

  [[nodiscard]] bool sameTick(const Output& b) const noexcept
  {
    return mItemsTick == b.mItemsTick && mQueryTick == b.mQueryTick;
  }
};

struct Job
{
  struct Reserved
  {
    alignas(kCacheLine) std::atomic<size_t> mReserved { 0 };
    size_t reserve(size_t n) noexcept { return mReserved.fetch_add(n); }
  };

  Items mItems;
  size_t mQueryTick { 0 };
  std::shared_ptr<std::string> mQuery;
  std::shared_ptr<Reserved> mReserved;

  [[nodiscard]] bool sameTick(const Output& b) const noexcept
  {
    return mItems.size() == b.mItemsTick && mQueryTick == b.mQueryTick;
  }
};

struct WorkerPool
{
  struct Worker
  {
    std::thread mThread;
    Events mEvents;
    Tx<Output> mOutput;
  };

  enum Event : uint32_t {
    kNone = 0,
    kStop = 1U << 0,
    kJob = 1U << 1,
    kMerge = 1U << 2,
  };

  void start(uint32_t workers);
  void stop();
  void notify() noexcept;

  [[nodiscard]] Worker* master()
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

  [[nodiscard]] const Worker* master() const
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

private:
  void run(uint8_t workerIndex);

public:
  LR<Job> mJob;
  EventFd mEventFd;
private:
  std::vector<std::unique_ptr<Worker>> mWorkers {};
};

} // namespace fzx
