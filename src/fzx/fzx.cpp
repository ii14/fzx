#include "fzx/fzx.hpp"

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "fzx/match/fzy.hpp"

using namespace std::string_view_literals;

namespace fzx {

Fzx::Fzx()
{
  if (pipe(mNotifyPipe) == -1) {
    perror("pipe");
    throw std::runtime_error { "pipe failed" };
  }

  if ((fcntl(mNotifyPipe[0], F_SETFD, FD_CLOEXEC) == -1) ||
      (fcntl(mNotifyPipe[1], F_SETFD, FD_CLOEXEC) == -1)) {
    perror("fcntl");
    throw std::runtime_error { "setting FD_CLOEXEC failed" };
  }

  if ((fcntl(mNotifyPipe[0], F_SETFL, O_NONBLOCK) == -1) ||
      (fcntl(mNotifyPipe[1], F_SETFL, O_NONBLOCK) == -1)) {
    perror("fcntl");
    throw std::runtime_error { "setting O_NONBLOCK failed" };
  }
}

Fzx::~Fzx() noexcept
{
  if (mNotifyPipe[0] >= 0)
    close(mNotifyPipe[0]);
  if (mNotifyPipe[1] >= 0)
    close(mNotifyPipe[1]);
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
  mEvents.post(Events::kStopEvent);
  if (mThread.joinable())
    mThread.join();
}

uint32_t Fzx::scanFeed(std::string_view s)
{
  uint32_t count = 0;
  const auto* it = s.begin();
  for (;;) {
    const auto* const nl = std::find(it, s.end(), '\n');
    const auto len = std::distance(it, nl);
    ASSUME(len >= 0);
    if (nl == s.end()) {
      if (len != 0) {
        const auto size = mScanBuffer.size();
        mScanBuffer.resize(size + len);
        std::memcpy(mScanBuffer.data() + size, it, len);
      }
      return count;
    } else if (len == 0) {
      if (!mScanBuffer.empty()) {
        mItems.push({ mScanBuffer.data(), mScanBuffer.size() });
        mScanBuffer.clear();
        ++count;
      }
    } else if (mScanBuffer.empty()) {
      mItems.push({ it, size_t(len) });
      ++count;
    } else {
      const auto size = mScanBuffer.size();
      mScanBuffer.resize(size + len);
      std::memcpy(mScanBuffer.data() + size, it, len);
      mItems.push({ mScanBuffer.data(), mScanBuffer.size() });
      mScanBuffer.clear();
      ++count;
    }
    it += len + 1;
  }
}

bool Fzx::scanEnd()
{
  const bool empty = mScanBuffer.empty();
  if (!empty)
    mItems.push({ mScanBuffer.data(), mScanBuffer.size() });
  mScanBuffer.clear();
  mScanBuffer.shrink_to_fit();
  return !empty;
}

void Fzx::commitItems() noexcept
{
  mItems.commit();
  mEvents.post(Events::kItemsEvent);
}

void Fzx::setQuery(std::string query) noexcept
{
  mQuery.writeBuffer() = std::move(query);
  mQuery.commit();
  mEvents.post(Events::kQueryEvent);
}

bool Fzx::loadResults() noexcept
{
  if (!mNotifyActive.exchange(false, std::memory_order_acquire))
    return false;
  char buf[32];
  UNUSED(read(mNotifyPipe[0], buf, std::size(buf)));
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
      if (hasMatch(mQuery, item))
        mResults.push_back({ uint32_t(i), float(match(mQuery, item)) });
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
    if (update & Events::kStopEvent)
      return;
    if (update & Events::kItemsEvent)
      reader.load();
    if (update & Events::kQueryEvent)
      if (!mQuery.load())
        update &= ~Events::kQueryEvent;
    if (update == Events::kNone)
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
      if (hasMatch(query, item))
        res.mResults.push_back({ uint32_t(i), float(fzx::match(query, item)) });
      if (++n == kCheckUpdate) {
        n = 0;
        update = mEvents.get();
        if (update != Events::kNone)
          goto again;
      }
    }

    std::sort(res.mResults.begin(), res.mResults.end());
#endif

    res.mItemsTick = reader.size();
    res.mQueryTick = mQuery.readTick();
    mResults.commit();
    if (!mNotifyActive.load(std::memory_order_relaxed) &&
        !mNotifyActive.exchange(true, std::memory_order_release)) {
      const char c = 0;
      UNUSED(write(mNotifyPipe[1], &c, 1));
    }
  }
}

} // namespace fzx
