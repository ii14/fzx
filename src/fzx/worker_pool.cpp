#include "fzx/worker_pool.hpp"

#include <algorithm>

#include "fzx/macros.hpp"
#include "fzx/match/fzy.hpp"
#include "fzx/thread.hpp"

namespace fzx {

// TODO: it's a mess, refactor this

namespace {

/// Check for a new job every N processed items
constexpr auto kCheckInterval = 0x8000;

// Results from each worker are merged using this dependency
// tree, where each vertical lane represents one worker thread:
//
//   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
//   |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
//   +--+  +--+  +--+  +--+  +--+  +--+  +--+  +--+
//   |  .  |  .  |  .  |  .  |  .  |  .  |  .  |  .
//   +-----+  .  +-----+  .  +-----+  .  +-----+  .
//   |  .  .  .  |  .  .  .  |  .  .  .  |  .  .  .
//   +-----------+  .  .  .  +-----------+  .  .  .
//   |  .  .  .  .  .  .  .  |  .  .  .  .  .  .  .
//   +-----------------------+  .  .  .  .  .  .  .
//   |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
//
// At each step the parent worker merges its results with the
// results from all the workers under it, until the full results
// end up in the worker 0, that then notifies the main thread.

/// Hard limit on 64 threads
constexpr auto kMaxWorkers = 64;
constexpr auto kMaxChildren = 6;

/// Map of worker index to the parent worker index. The parent is responsible for
/// merging your results. It has to be notified about results being ready to merge.
constexpr std::array<uint8_t, kMaxWorkers> kParentMap {
  // 0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
  0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x04, 0x06, 0x00, 0x08, 0x08, 0x0A, 0x08, 0x0C, 0x0C, 0x0E,
  0x00, 0x10, 0x10, 0x12, 0x10, 0x14, 0x14, 0x16, 0x10, 0x18, 0x18, 0x1A, 0x18, 0x1C, 0x1C, 0x1E,
  0x00, 0x20, 0x20, 0x22, 0x20, 0x24, 0x24, 0x26, 0x20, 0x28, 0x28, 0x2A, 0x28, 0x2C, 0x2C, 0x2E,
  0x20, 0x30, 0x30, 0x32, 0x30, 0x34, 0x34, 0x36, 0x30, 0x38, 0x38, 0x3A, 0x38, 0x3C, 0x3C, 0x3E,
};

/// Map of worker index to max children count.
constexpr std::array<uint8_t, kMaxWorkers> kMaxChildrenMap {
  6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
};

/// Get child worker index.
constexpr uint8_t nthChild(uint8_t id, uint8_t n) noexcept
{
  DEBUG_ASSERT(id < kMaxWorkers);
  DEBUG_ASSERT(n <= kMaxChildren);
  return id + (1U << n);
}

/// Get child merge mask.
constexpr uint8_t nthChildMask(uint8_t n) noexcept
{
  DEBUG_ASSERT(n <= kMaxChildren);
  return 1U << n;
}

/// Get children count.
constexpr uint8_t childrenCount(uint8_t id, uint8_t total) noexcept
{
  DEBUG_ASSERT(id < kMaxWorkers);
  DEBUG_ASSERT(total <= kMaxWorkers);
  uint8_t i = 0;
  while (i < kMaxChildrenMap[id] && nthChild(id, i) < total)
    ++i;
  DEBUG_ASSERT(i <= kMaxChildren);
  return i;
}

/// Get empty merge mask.
constexpr uint8_t childrenMask(uint8_t count) noexcept
{
  DEBUG_ASSERT(count <= kMaxChildren);
  return ~(0xFFU << count);
}

/// Merge results from sorted vectors `a` and `b` into the output vector `r`.
/// `r` is an in/out parameter to reuse previously allocated memory.
void merge2(
    std::vector<Match>& RESTRICT r,
    const std::vector<Match>& RESTRICT a,
    const std::vector<Match>& RESTRICT b)
{
  r.clear();
  r.resize(a.size() + b.size());
  const auto ae = a.end();
  const auto be = b.end();
  auto ai = a.begin();
  auto bi = b.begin();
  auto ri = r.begin();
  while (ai != ae && bi != be)
    *ri++ = *ai < *bi ? *ai++ : *bi++;
  while (ai != ae)
    *ri++ = *ai++;
  while (bi != be)
    *ri++ = *bi++;
  DEBUG_ASSERT(ri == r.end());
}

} // namespace

void WorkerPool::start(uint32_t workers)
{
  ASSERT(workers <= kMaxWorkers);
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

void WorkerPool::notify() noexcept
{
  for (auto& worker : mWorkers)
    worker->mEvents.post(kJob);
}

void WorkerPool::run(const uint32_t workerIndex)
{
  // TODO: exception safety

  ASSERT(workerIndex < kMaxWorkers);
  ASSERT(mWorkers.size() <= kMaxWorkers);

  Thread::pin(static_cast<int>(workerIndex));

  auto& worker = mWorkers[workerIndex];
  auto& output = worker->mOutput;
  auto& events = worker->mEvents;

  Job job;
  size_t lastItemsTick = 0;
  size_t lastQueryTick = 0;

  // Temporary vector for merging results
  std::vector<Match> tmp;

  const uint8_t kParentIndex = kParentMap[workerIndex];
  const uint8_t kChildrenCount = childrenCount(workerIndex, mWorkers.size());
  // Keep track of what results have been merged
  const uint8_t kChildrenMask = childrenMask(kChildrenCount);
  uint8_t merged = kChildrenMask;

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
      mWorkers[kParentIndex]->mEvents.post(kMerge);
    }
  };

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
    merged = kChildrenMask;

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
  if (merged != 0) {
    auto& out = output.writeBuffer();

    for (uint8_t i = 0; i < kChildrenCount; ++i) {
      const uint8_t id = nthChild(workerIndex, i);
      const uint8_t mask = nthChildMask(i);

      // Already got results from this worker.
      if (!(merged & mask))
        continue;

      mWorkers[id]->mOutput.load();
      const auto& cres = mWorkers[id]->mOutput.readBuffer();

      if (cres.mItemsTick > out.mItemsTick || cres.mQueryTick > out.mQueryTick) {
        // We've received results with a newer timestamp than ours, so that has to mean there
        // is a new job. We know something's up, but we can't read the events here, because even
        // though the other worker was notified and did its thing, there is no guarantee that
        // _we_ were notified yet, as workers are not notified simultaneously. We have to wait.
        // I mean I guess we could, but we have to process all events again anyway, just in case.
        goto wait;
      } else if (cres.mItemsTick == out.mItemsTick && cres.mQueryTick == out.mQueryTick) {
        // We agree on the timestamp, the results can be merged.
        merge2(tmp, out.mItems, cres.mItems); // TODO: merge in place?
        swap(tmp, out.mItems);
        merged &= ~mask; // Mark this worker as merged.
      }
    }

    // Not all results have been merged yet, we have to wait for someone.
    if (merged != 0)
      goto wait;
  }

  // Publish results.
  publish();
  goto wait;
}

} // namespace fzx
