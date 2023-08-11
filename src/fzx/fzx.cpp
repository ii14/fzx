#include "fzx/fzx.hpp"

#include <stdexcept>

#include "fzx/line_scanner.ipp"
#include "fzx/macros.hpp"

namespace fzx {

Fzx::Fzx()
{
  if (auto err = mPool.mEventFd.open(); !err.empty())
    throw std::runtime_error { err };
}

Fzx::~Fzx() noexcept
{
  stop();
  mPool.mEventFd.close();
}

void Fzx::setThreads(unsigned threads) noexcept
{
  mThreads = std::clamp(threads, 1U, 64U);
}

void Fzx::start()
{
  ASSERT(!mRunning);
  mRunning = true;
  mPool.start(mThreads);
}

void Fzx::stop() noexcept
{
  if (!mRunning)
    return;
  mRunning = false;
  mPool.stop();
}

uint32_t Fzx::scanFeed(std::string_view s)
{
  return mLineScanner.feed(s, [this](std::string_view item) {
    mJob.mItems.push(item);
  });
}

bool Fzx::scanEnd()
{
  return mLineScanner.finalize([this](std::string_view item) {
    mJob.mItems.push(item);
  });
}

void Fzx::commitItems() noexcept
{
  // Suppress unnecessary notifications
  if (mJob.mItems.size() <= mLastItemsSize)
    return;
  mLastItemsSize = mJob.mItems.size();

  // If there is an active query, we're considering this to be a new job
  // and the counter has to be reset, and all workers can start over.
  if (mJob.mQuery)
    mJob.mReserved = std::make_shared<Job::Reserved>();

  mPool.mJob.store(mJob);
  mPool.notify();
}

void Fzx::setQuery(std::string query) noexcept
{
  if (!query.empty()) {
    mJob.mQuery = std::make_shared<std::string>(std::move(query));
    mJob.mReserved = std::make_shared<Job::Reserved>();
  } else {
    mJob.mQuery.reset();
    mJob.mReserved.reset();
  }
  mJob.mQueryTick += 1;

  mPool.mJob.store(mJob);
  mPool.notify();
}

[[nodiscard]] std::string_view Fzx::query() const
{
  if (const auto* master = mPool.master(); master != nullptr)
    if (const auto& out = master->mOutput.readBuffer(); out.mQuery)
      return *out.mQuery;
  return {};
}

bool Fzx::loadResults() noexcept
{
  mPool.mEventFd.consume();
  if (auto* master = mPool.master(); master != nullptr)
    return master->mOutput.load();
  return false;
}

size_t Fzx::resultsSize() const noexcept
{
  if (const auto* master = mPool.master(); master != nullptr)
    if (const auto& out = master->mOutput.readBuffer(); out.mQuery)
      return out.mItems.size();
  return mJob.mItems.size();
}

Result Fzx::getResult(size_t i) const noexcept
{
  if (const auto* master = mPool.master(); master != nullptr) {
    if (const auto& out = master->mOutput.readBuffer(); out.mQuery) {
      DEBUG_ASSERT(i < out.mItems.size());
      if (i >= out.mItems.size())
        return {};
      const auto& match = out.mItems[i];
      return { mJob.mItems.at(match.mIndex), match.mIndex, match.mScore };
    }
  }

  DEBUG_ASSERT(i < mJob.mItems.size());
  if (i >= mJob.mItems.size())
    return {};
  return { mJob.mItems.at(i), static_cast<uint32_t>(i), 0 };
}

bool Fzx::processing() const noexcept
{
  if (!mJob.mQuery)
    return false;
  if (const auto* master = mPool.master(); master != nullptr)
    return !mJob.sameTick(master->mOutput.readBuffer());
  return false;
}

double Fzx::progress() const noexcept
{
  if (!mJob.mReserved)
    return 1.0;
  // This atomic counter can include items that are about to be processed. Also
  // this doesn't include sorting. It's fine though, this is just an approximation.
  const size_t processed = mJob.mReserved->mReserved.load(std::memory_order_relaxed);
  const size_t total = mJob.mItems.size();
  return static_cast<double>(std::min(processed, total)) / static_cast<double>(total);
}

} // namespace fzx
