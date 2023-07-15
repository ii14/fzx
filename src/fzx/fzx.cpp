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

void Fzx::start()
{
  ASSERT(!mRunning);
  mRunning = true;
  mPool.start(4);
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
  mPool.mJob.store(mJob);
  mPool.notify();
}

void Fzx::setQuery(std::string query) noexcept
{
  if (!query.empty()) {
    mJob.mQuery = std::make_shared<std::string>(std::move(query));
  } else {
    mJob.mQuery.reset();
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
  const auto* master = mPool.master();
  if (master == nullptr)
    return false;
  const auto& out = master->mOutput.readBuffer();
  return mJob.mItems.size() != out.mItemsTick || mJob.mQueryTick != out.mQueryTick;
}

} // namespace fzx
