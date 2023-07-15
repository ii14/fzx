#include "fzx/worker_pool.hpp"

#include <algorithm>

#include "fzx/macros.hpp"
#include "fzx/match/fzy.hpp"
#include "fzx/thread.hpp"

namespace fzx {

// TODO: it's a mess, refactor this

void WorkerPool::start(uint32_t workers)
{
  for (size_t i = 0; i < workers; ++i)
    mWorkers.emplace_back(std::make_unique<Worker>());
  for (size_t i = 0; i < mWorkers.size(); ++i) {
    auto& worker = mWorkers[i];
    worker->mThread = std::thread { &WorkerPool::run, this, static_cast<uint32_t>(i) };
  }
}

void WorkerPool::stop()
{
  for (auto& worker : mWorkers)
    worker->mEvents.post(kStop);
  for (auto& worker : mWorkers)
    if (worker->mThread.joinable())
      worker->mThread.join();
  mWorkers.clear();
}

void WorkerPool::notify()
{
  for (auto& worker : mWorkers)
    worker->mEvents.post(kJob);
}

namespace {

void merge2(
    std::vector<Match>& out,
    const std::vector<Match>& a,
    const std::vector<Match>& b)
{
  size_t ir = 0;
  size_t ia = 0;
  size_t ib = 0;
  out.clear();
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
}

// TODO: try to avoid __builtin functions

constexpr uint8_t emptyMergedMask(uint32_t id, size_t size) noexcept
{
  // TODO: document what this does

  uint32_t max = [&] {
    if (id != 0)
      return __builtin_ffs(static_cast<int>(id)) - 1;
    static_assert(sizeof(int) == 4);
    // Shifting in 1, because the result for 0 is different on different CPUs. thank you intel, very cool
    return 31 - __builtin_clz((static_cast<int>(size) << 1) | 1);
  }();

  uint32_t r = 0;
  for (uint32_t i = 0; i < max && (id | (1U << i)) < size; ++i)
    ++r;
  return ~(0xFF << r);
}

} // namespace

void WorkerPool::run(uint32_t workerIndex)
{
  // TODO: exception safety
  ASSERT(workerIndex < 64); // Hard limit on 64 threads

  Thread::pin(static_cast<int>(workerIndex));

  auto& worker = mWorkers[workerIndex];
  auto& output = worker->mOutput;
  auto& events = worker->mEvents;

  Job job;
  size_t lastItemsTick = 0;
  size_t lastQueryTick = 0;

  // Temporary vector for merging results
  std::vector<Match> tmp;
  // Keep track of what results have been merged
  const uint8_t kEmptyMergedMask = emptyMergedMask(workerIndex, mWorkers.size());
  uint8_t mergedMask = kEmptyMergedMask;

  bool published = false;
  // Commit results and notify whoever is responsible for handling them.
  auto publish = [&] {
    if (published)
      return;
    published = true;
    output.commit();

    if (workerIndex == 0) {
      // Worker 0 is the main thread, publish results globally
      mEventFd.notify();
    } else {
      // For all other workers, notify the worker that is responsible for merging our results.
      // credits to sean's leetcode skills for figuring this one out
      // TODO: document what this does
      const int fs = __builtin_ffs(static_cast<int>(workerIndex));
      const uint32_t id = workerIndex & ~(1U << (fs - 1));
      mWorkers[id]->mEvents.post(kMerge);
    }
  };

  // Check for a new job every N processed items
  constexpr auto kCheckInterval = 0x8000;

  // TODO: Not sure how to express this control flow, so use gotos for now.
  // We can make it a proper clean code that is even harder to follow than gotos later.
wait:
  uint32_t ev = events.wait();
start:
  if (ev & kStop)
    return;

  if (ev & kJob) {
    mJob.load(job);

    // Temporarily unset kJob event, and reenable it if we actually have a new job.
    ev &= ~kJob;

    if (lastQueryTick < job.mQueryTick) {
      lastQueryTick = job.mQueryTick;
      ev |= kJob;
    }

    // TODO: If the query didn't change, results should't have to be recalculated,
    // not fully at least. Only the new items can be matched, sorted and then sort
    // everything again. What can make it slightly more complicated is the fact that
    // the items are evenly split across workers.
    if (lastItemsTick < job.mItems.size()) {
      lastItemsTick = job.mItems.size();
      ev |= kJob;
    }
  }

  if (ev & kJob) {
    // A new job invalidates any merged results we got so far.
    published = false;
    mergedMask = kEmptyMergedMask;

    auto& out = output.writeBuffer();

    // Prepare results. Start from scratch with a new item vector and "timestamp" the results.
    out.mItemsTick = job.mItems.size();
    out.mQueryTick = job.mQueryTick;
    out.mQuery = job.mQuery;
    out.mItems.clear();

    // If there is no active query, publish empty results.
    if (!job.mQuery || job.mQuery->empty()) {
      // On the other side we need to know if we should just return the
      // items straight from the Items data structure, if query is empty.
      publish();
      goto wait;
    }

    // Each thread gets its own chunk of the items.
    const auto chunkSize = job.mItems.size() / mWorkers.size() + 1;
    const size_t start = std::min(chunkSize * workerIndex, job.mItems.size());
    const size_t end = std::min(start + chunkSize, job.mItems.size());
    if (start < end) {
      // Match items and calculate scores
      out.mItems.reserve(end - start);
      const std::string_view query { *job.mQuery };
      size_t n = 0;
      for (size_t i = start; i < end; ++i) {
        auto item = job.mItems.at(i);
        if (!fzy::hasMatch(query, item))
          continue;
        out.mItems.push_back({
          static_cast<uint32_t>(i),
          static_cast<float>(fzy::match(query, item)),
        });

        // Periodically look for a new job.
        if (++n == kCheckInterval) {
          n = 0;
          // Ignore kMerge events, we don't care about them at this stage.
          if (ev = events.get(); ev & (kStop | kJob))
            goto start;
        }
      }

      // Sort the local batch of items.
      std::sort(out.mItems.begin(), out.mItems.end());
    }
  }

  // Merge results from other threads.
  if (mergedMask != 0) {
    auto& out = output.writeBuffer();

    // TODO: document what this does
    const uint32_t max = workerIndex == 0
      ? mWorkers.size()
      : 1U << (__builtin_ffs(static_cast<int>(workerIndex)) - 1);
    for (uint32_t i = 0, n = 1; n < max && (workerIndex | n) < mWorkers.size(); ++i, n <<= 1) {
      const uint32_t id = workerIndex | n;
      const uint8_t mask = 1U << i;

      // Already got results from this worker.
      if (!(mergedMask & mask))
        continue;

      mWorkers[id]->mOutput.load();
      const auto& cres = mWorkers[id]->mOutput.readBuffer();

      if (cres.mItemsTick > out.mItemsTick || cres.mQueryTick > out.mQueryTick) {
        // We've received results with a newer timestamp than ours, so that has to mean there
        // is a new job. We know something's up, but we can't read the events here, because even
        // though the other worker was notified and did its thing, there is no guarantee that
        // _we_ were notified yet, as workers are not notified simultaneously. We have to wait.
        // I mean I guess we could, but we have to process all events again anyway.
        goto wait;
      } else if (cres.mItemsTick == out.mItemsTick && cres.mQueryTick == out.mQueryTick) {
        // We agree on the timestamp, the results can be merged.
        merge2(tmp, out.mItems, cres.mItems); // TODO: merge in place?
        swap(tmp, out.mItems);
        mergedMask &= ~mask; // Mark this worker as merged.
      }
    }

    // Not all results have been merged yet, we have to wait for someone.
    if (mergedMask != 0)
      goto wait;
  }

  // Publish results.
  publish();
  goto wait;
}

} // namespace fzx
