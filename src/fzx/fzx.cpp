#include "fzx/fzx.hpp"

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

#include "fzx/match/fzy.hpp"

#ifndef _GNU_SOURCE
static int pipe2(int fildes[2], int flags)
{
  int ret = 0;

  ret = pipe(fildes);
  if (ret == -1) {
    return ret;
  }

  if ((flags & O_CLOEXEC) != 0) {
    ret = fcntl(fildes[0], F_SETFD, FD_CLOEXEC);
    if (ret == -1) {
      return ret;
    }

    ret = fcntl(fildes[1], F_SETFD, FD_CLOEXEC);
    if (ret == -1) {
      return ret;
    }
  }

  if ((flags & O_NONBLOCK) != 0) {
    ret = fcntl(fildes[0], F_SETFL, O_NONBLOCK);
    if (ret == -1) {
      return ret;
    }

    ret = fcntl(fildes[1], F_SETFL, O_NONBLOCK);
    if (ret == -1) {
      return ret;
    }
  }

  return 0;
}
#endif

using namespace std::string_view_literals;

namespace fzx {

enum Update : uint32_t {
  kNone = 0,
  kStop = 1U << 0,
  kItems = 1U << 1,
  kQuery = 1U << 2,
};

Fzx::Fzx()
{
  if (pipe2(mNotifyPipe, O_NONBLOCK | O_CLOEXEC) == -1) {
    perror("pipe2");
    throw std::runtime_error { "pipe2 failed" };
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
  mUpdate.fetch_or(Update::kStop, std::memory_order_release);
  mCv.notify_one();
  mThread.join();
}

void Fzx::commitItems() noexcept
{
  mItems.commit();
  mUpdate.fetch_or(Update::kItems, std::memory_order_release);
  mCv.notify_one();
}

void Fzx::setQuery(std::string query) noexcept
{
  mQuery.writeBuffer() = std::move(query);
  mQuery.commit();
  mUpdate.fetch_or(Update::kQuery, std::memory_order_release);
  mCv.notify_one();
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
  const auto& matches = mResults.readBuffer();
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
        mResults.push_back({ i, match(mQuery, item) });
    }
    std::sort(mResults.begin(), mResults.end());
  }
};

void Fzx::run()
{
  uint32_t update = Update::kNone;
  auto reader = mItems.reader();

wait:
  std::unique_lock lock { mMutex };
  mCv.wait(lock, [&] {
    update = mUpdate.exchange(Update::kNone, std::memory_order_acquire);
    return update != Update::kNone;
  });

again:
  if (update & Update::kStop)
    return;
  if (update & Update::kItems)
    reader.load();
  if (update & Update::kQuery)
    if (!mQuery.load())
      update &= ~Update::kQuery;

  if (update != Update::kNone) {
    std::string_view query = mQuery.readBuffer();
    auto& res = mResults.writeBuffer();
    res.clear();

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
    merge2(res, r1, r2);
#else
    res.reserve(reader.size());
    constexpr auto kCheckUpdate = 0x2000;

    size_t n = 0;
    for (size_t i = 0; i < reader.size(); ++i) {
      auto item = reader[i];
      if (hasMatch(query, item))
        res.push_back({ i, match(query, item) });
      if (++n == kCheckUpdate) {
        n = 0;
        update = mUpdate.exchange(Update::None, std::memory_order_acquire);
        if (update != Update::None)
          goto again;
      }
    }

    std::sort(res.begin(), res.end(), matchCompare);
#endif

    mResults.commit();
    if (!mNotifyActive.load(std::memory_order_relaxed) &&
        !mNotifyActive.exchange(true, std::memory_order_release)) {
      const char c = 0;
      UNUSED(write(mNotifyPipe[1], &c, 1));
    }
  }

  update = mUpdate.exchange(Update::kNone, std::memory_order_acquire);
  if (update != Update::kNone)
    goto again;
  goto wait;
}

} // namespace fzx
