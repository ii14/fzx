// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/fzx.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>

#include "fzx/config.hpp"
#include "fzx/macros.hpp"

namespace fzx {

Fzx::Fzx()
{
  setThreads(std::thread::hardware_concurrency());
}

Fzx::~Fzx() noexcept
{
  stop();
}

void Fzx::setCallback(Callback callback, void* userData) noexcept
{
  ASSERT(!mRunning);
  mCallback = callback;
  mUserData = userData;
}

void Fzx::setThreads(unsigned threads) noexcept
{
  mThreads = std::clamp(threads, 1U, kMaxThreads);
}

void Fzx::start()
{
  if (mRunning)
    return;
  mRunning = true;

  ASSERT(mCallback);

  for (size_t i = 0; i < mThreads; ++i) {
    auto& worker = mWorkers.emplace_back(std::make_unique<Worker>());
    worker->mIndex = i;
    worker->mPool = this;
  }
  for (const auto& worker : mWorkers)
    worker->mThread = std::thread { &Worker::run, worker.get() };
}

void Fzx::stop()
{
  if (!mRunning)
    return;
  mRunning = false;

  for (auto& worker : mWorkers)
    worker->mEvents.post(Worker::kStop);
  for (auto& worker : mWorkers)
    if (worker->mThread.joinable())
      worker->mThread.join();
  mWorkers.clear();
}

void Fzx::setQuery(std::string_view query)
{
  if (mQuery && mQuery->str() == query)
    return;
  if (!query.empty()) {
    mQuery = std::make_shared<AlignedString>(query);
  } else {
    mQuery.reset();
  }
  commit();
}

void Fzx::commit()
{
  // mJob is protected by the shared mutex, however only this thread can
  // modify it, so it's safe for us to read it before acquiring a lock.
  bool queryChanged = mJob.mQuery.get() != mQuery.get();
  bool itemsChanged = mJob.mItems.size() != mItems.size();
  if (!queryChanged && !itemsChanged)
    return;

  // TODO: Don't wake up workers when items changed but there is no active query.
  // Or wake them up only once when deleting the query, in case they were doing
  // something and they can stop now. If items changed though, the external event
  // loop should still be notified about it. Figure out if EventFd::notify is safe
  // to call from multiple threads at once.

  bool queueChanged = queryChanged || (itemsChanged && mQuery);
  if (queueChanged) {
    if (mQuery) {
      mQueue = std::make_shared<ItemQueue>();
    } else {
      mQueue.reset();
    }
  }

  {
    std::unique_lock lock { mJobMutex };
    if (itemsChanged)
      mJob.mItems = mItems;
    if (queueChanged)
      mJob.mQueue = mQueue;
    if (queryChanged) {
      ++mJob.mQueryTick;
      mJob.mQuery = mQuery;
    }
  }

  // Wake up worker threads
  for (auto& worker : mWorkers)
    worker->mEvents.post(Worker::kJob);
}

bool Fzx::loadResults() noexcept
{
  // TODO: mEventFd.consume();
  if (Worker* master = masterWorker(); master != nullptr)
    return master->mOutput.load();
  return false;
}

size_t Fzx::resultsSize() const noexcept
{
  if (const Results* res = getResults(); res != nullptr && res->mQuery)
    return res->mItems.size();
  return mItems.size();
}

Result Fzx::getResult(size_t i) const noexcept
{
  if (const Results* res = getResults(); res != nullptr && res->mQuery) {
    DEBUG_ASSERT(i < res->mItems.size());
    if (i >= res->mItems.size())
      return {};
    const Match& match = res->mItems[i];
    return { mItems.at(match.index()), match.index(), match.score() * fzy::kScoreMultiplier };
  } else {
    DEBUG_ASSERT(i < mItems.size());
    if (i >= mItems.size())
      return {};
    return { mItems.at(i), static_cast<uint32_t>(i), 0 };
  }
}

std::string_view Fzx::query() const
{
  if (const Results* res = getResults(); res != nullptr && res->mQuery)
    return res->mQuery->str();
  return {};
}

bool Fzx::processing() const noexcept
{
  if (!mQuery)
    return false;
  if (const Results* res = getResults(); res != nullptr)
    return mItems.size() != res->mItemsTick || mQuery.get() != res->mQuery.get();
  return false;
}

double Fzx::progress() const noexcept
{
  if (!mQueue)
    return 1.0;
  // This atomic counter can include items that are about to be processed. Also
  // this doesn't include sorting. It's fine though, this is just an approximation.
  const size_t processed = mQueue->get();
  const size_t total = mItems.size();
  return static_cast<double>(std::min(processed, total)) / static_cast<double>(total);
}

bool Fzx::synchronized() const noexcept
{
  if (const Results* res = getResults(); res != nullptr)
    return mItems.size() == res->mItemsTick && mQuery.get() == res->mQuery.get();
  return true;
}

} // namespace fzx
