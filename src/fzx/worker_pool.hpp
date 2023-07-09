#pragma once

#include <array>
#include <thread>
#include <vector>
#include <memory>

#include "fzx/eventfd.hpp"
#include "fzx/events.hpp"
#include "fzx/items.hpp"
#include "fzx/lr.hpp"
#include "fzx/tx_value.hpp"
#include "fzx/match.hpp"

namespace fzx {

struct Output
{
  std::vector<Match> mItems;
  size_t mItemsTick { 0 };
  size_t mQueryTick { 0 };
  bool mNoQuery { true };
};

struct Job
{
  Items mItems;
  size_t mQueryTick { 0 };
  std::shared_ptr<std::string> mQuery;
};

struct WorkerPool
{
  struct Worker
  {
    std::thread mThread;
    Events mEvents;
    TxValue<Output> mOutput;
  };

  enum Event : uint32_t {
    kNone = 0,
    kStop = 1U << 0,
    kJob = 1U << 1,
    kMerge = 1U << 2,
  };

  void start(uint32_t workers);
  void stop();
  void notify();

  [[nodiscard]] Worker* master()
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

  [[nodiscard]] const Worker* master() const
  {
    return mWorkers.empty() ? nullptr : mWorkers.front().get();
  }

private:
  void run(uint32_t workerIndex);

public:
  LR<Job> mJob;
  EventFd mEventFd;
private:
  std::vector<std::unique_ptr<Worker>> mWorkers {};
};

} // namespace fzx
