#include "fzx/fzx.hpp"

#include "fzx/match/fzy.hpp"
#include "fzx/line_scanner.ipp"

namespace fzx {

enum Event : uint32_t {
  kNone = 0,
  kStopEvent = 1U << 0,
  kItemsEvent = 1U << 1,
  kQueryEvent = 1U << 2,
};

Fzx::Fzx()
{
  if (auto err = mEventFd.open(); !err.empty())
    throw std::runtime_error { err };
}

Fzx::~Fzx() noexcept
{
  mEventFd.close();
  stop();
}

void Fzx::start()
{
  ASSERT(!mRunning);
  mRunning = true;
  mThread = std::thread { &Fzx::run, this };
}

void Fzx::stop() noexcept
{
  if (!mRunning)
    return;
  mRunning = false;
  mEvents.post(Event::kStopEvent);
  if (mThread.joinable())
    mThread.join();
}

uint32_t Fzx::scanFeed(std::string_view s)
{
  return mLineScanner.feed(s, [this](std::string_view item) {
    mItems.push(item);
  });
}

bool Fzx::scanEnd()
{
  return mLineScanner.finalize([this](std::string_view item) {
    mItems.push(item);
  });
}

void Fzx::commitItems() noexcept
{
  mItems.commit();
  mEvents.post(Event::kItemsEvent);
}

void Fzx::setQuery(std::string query) noexcept
{
  mQuery.writeBuffer() = std::move(query);
  mQuery.commit();
  mEvents.post(Event::kQueryEvent);
}

bool Fzx::loadResults() noexcept
{
  mEventFd.consume();
  return mResults.load();
}

Result Fzx::getResult(size_t i) const noexcept
{
  const auto& matches = mResults.readBuffer().mResults;
  ASSERT(i < matches.size());
  const auto& match = matches[i];
  return { mItems.at(match.mIndex), match.mScore };
}

struct Job
{
  std::string_view mQuery;
  const ItemReader* mReader { nullptr };
  std::vector<Match> mResults;
  size_t mStart { 0 };
  size_t mEnd { 0 };
  std::thread mThread;

  void run()
  {
    mResults.clear();
    if (mEnd <= mStart)
      return;
    mResults.reserve(mEnd - mStart);
    for (size_t i = mStart; i < mEnd; ++i) {
      auto item = mReader->at(i);
      if (fzy::hasMatch(mQuery, item))
        mResults.push_back({
          static_cast<uint32_t>(i),
          static_cast<float>(fzy::match(mQuery, item)),
        });
    }
    std::sort(mResults.begin(), mResults.end());
  }
};

void Fzx::run()
{
  // TODO: exception safety
  auto reader = mItems.reader();

  while (true) {
// again:
    uint32_t update = mEvents.wait();
    if (update & Event::kStopEvent)
      return;
    if (update & Event::kItemsEvent)
      reader.load();
    if (update & Event::kQueryEvent)
      if (!mQuery.load())
        update &= ~Event::kQueryEvent;
    if (update == Event::kNone)
      continue;

    std::string_view query = mQuery.readBuffer();
    auto& res = mResults.writeBuffer();
    res.mResults.clear();

#if 1
    constexpr auto kThreads = 4;
    const auto chunkSize = reader.size() / kThreads + 1;
    std::array<Job, kThreads> jobs;
    for (size_t n = 0; n < jobs.size(); ++n) {
      auto& job = jobs[n];
      job.mQuery = query;
      job.mReader = &reader;
      job.mStart = chunkSize * n;
      job.mEnd = std::min(chunkSize * n + chunkSize, reader.size());
      // TODO: make a thread pool instead of creating threads here
      if (n > 0)
        job.mThread = std::thread { &Job::run, &job };
    }

    jobs[0].run();
    for (size_t n = 1; n < jobs.size(); ++n)
      jobs[n].mThread.join();

    auto merge2 = [](
        std::vector<Match>& out,
        const std::vector<Match>& a,
        const std::vector<Match>& b)
    {
      size_t ir = 0;
      size_t ia = 0;
      size_t ib = 0;
      out.resize(a.size() + b.size());
      while (ia < a.size() && ib < b.size()) {
        if (a[ia] < b[ib]) {
          out[ir++] = a[ia++];
        } else {
          out[ir++] = b[ib++];
        }
      }
      while (ia < a.size())
        out[ir++] = a[ia++];
      while (ib < b.size())
        out[ir++] = b[ib++];
    };

    std::vector<Match> r1;
    std::vector<Match> r2;
    merge2(r1, jobs[0].mResults, jobs[1].mResults);
    merge2(r2, jobs[2].mResults, jobs[3].mResults);
    merge2(res.mResults, r1, r2);
#else
    res.mResults.reserve(reader.size());
    constexpr auto kCheckUpdate = 0x2000;

    size_t n = 0;
    for (size_t i = 0; i < reader.size(); ++i) {
      auto item = reader[i];
      if (fzy::hasMatch(query, item))
        res.mResults.push_back({
          static_cast<uint32_t>(i),
          static_cast<float>(fzy::match(query, item)),
        });
      if (++n == kCheckUpdate) {
        n = 0;
        update = mEvents.get();
        if (update != Event::kNone)
          goto again;
      }
    }

    std::sort(res.mResults.begin(), res.mResults.end());
#endif

    res.mItemsTick = reader.size();
    res.mQueryTick = mQuery.readTick();
    mResults.commit();
    mEventFd.notify();
  }
}

} // namespace fzx
