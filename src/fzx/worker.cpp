// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/worker.hpp"

#include <algorithm>
#include <cstring>

#include "fzx/config.hpp"
#include "fzx/fzx.hpp"
#include "fzx/macros.hpp"
#include "fzx/match/fzy/fzy.hpp"
#include "fzx/match/match.hpp"

namespace fzx {

namespace {

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

static_assert(kMaxThreads == 64);
/// Map of worker index to the parent worker index. The parent is responsible for
/// merging your results. It has to be notified about results being ready to merge.
constexpr std::array<uint8_t, kMaxThreads> kParentMap {
  // 0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
  0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x04, 0x06, 0x00, 0x08, 0x08, 0x0A, 0x08, 0x0C, 0x0C, 0x0E,
  0x00, 0x10, 0x10, 0x12, 0x10, 0x14, 0x14, 0x16, 0x10, 0x18, 0x18, 0x1A, 0x18, 0x1C, 0x1C, 0x1E,
  0x00, 0x20, 0x20, 0x22, 0x20, 0x24, 0x24, 0x26, 0x20, 0x28, 0x28, 0x2A, 0x28, 0x2C, 0x2C, 0x2E,
  0x20, 0x30, 0x30, 0x32, 0x30, 0x34, 0x34, 0x36, 0x30, 0x38, 0x38, 0x3A, 0x38, 0x3C, 0x3C, 0x3E,
};

/// Keep track of results from what workers have been merged in a bitset.
struct MergeState
{
  MergeState(const uint8_t workerIndex, const size_t workersCount) noexcept
    : mIndex(workerIndex)
  {
    ASSERT(workerIndex < kMaxThreads);
    ASSERT(workersCount <= kMaxThreads);

    constexpr auto kMaxChildren = 6;
    // Map of worker index to max possible children count
    static constexpr std::array<uint8_t, kMaxThreads> kMaxChildrenMap {
      6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
      5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    };

    while (mCount < kMaxChildrenMap[workerIndex] && workerIndex + (1U << mCount) < workersCount)
      ++mCount;
    ASSERT(mCount <= kMaxChildren);

    mMask = ~(0xFFU << mCount);
    mState = mMask;
  }

  // Get children count.
  [[nodiscard]] uint8_t size() const noexcept
  {
    return mCount;
  }

  [[nodiscard]] uint8_t at(uint8_t child) const noexcept
  {
    DEBUG_ASSERT(child < mCount);
    return mIndex + (1U << child);
  }

  // Reset state.
  void reset() noexcept
  {
    mState = mMask;
  }

  // Mark nth child as merged.
  void set(uint8_t child) noexcept
  {
    DEBUG_ASSERT(child < mCount);
    mState &= ~(1U << child);
  }

  // Check if results from all children were merged.
  [[nodiscard]] bool done() const noexcept
  {
    return mState == 0;
  }

  // Check if results from nth child were merged.
  [[nodiscard]] bool contains(uint8_t child) const noexcept
  {
    DEBUG_ASSERT(child < mCount);
    return !(mState & (1U << child));
  }

private:
  uint8_t mIndex { 0 }; ///< Current worker index
  uint8_t mCount { 0 }; ///< Children count
  uint8_t mMask { 0 }; ///< Children mask
  uint8_t mState { 0 }; ///< Merge state
};

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

// TODO: compile time option to disable try/catch
void Worker::run() try
{
  ASSERT(mIndex < mPool->mWorkers.size());
  ASSERT(mPool->mWorkers.size() <= kMaxThreads);

  auto& output = mPool->mWorkers[mIndex]->mOutput;
  auto& events = mPool->mWorkers[mIndex]->mEvents;

  Job job;
  size_t lastItemsTick = 0;
  size_t lastQueryTick = 0;

  // Temporary vector for merging results
  std::vector<Match> tmp;

  const uint8_t kParentIndex = kParentMap[mIndex];
  MergeState mergeState { mIndex, mPool->mWorkers.size() };

  bool published = false;
  // Commit results and notify whoever is responsible for handling them.
  auto publish = [&] {
    if (published)
      return;
    published = true;
    output.commit();

    if (mIndex == 0) {
      // Worker 0 is the master worker thread. Notify the external event loop.
      mPool->mCallback(mPool->mUserData);
    } else {
      // For all other workers, notify the worker that is responsible for merging our results.
      mPool->mWorkers[kParentIndex]->mEvents.post(kMerge);
    }
  };

  // TODO: Not sure how to express this control flow, so use gotos for now.
  // We can make it a proper clean code that is even harder to follow than gotos later.
wait:
  uint32_t ev = events.wait();
start:
  if (ev & kStop)
    return;

  bool itemsChanged = false;
  bool queryChanged = false;

  if (ev & kJob) {
    mPool->loadJob(job);

    if (lastItemsTick < job.mItems.size()) {
      lastItemsTick = job.mItems.size();
      itemsChanged = true;
    }

    if (lastQueryTick < job.mQueryTick) {
      lastQueryTick = job.mQueryTick;
      queryChanged = true;
    }
  }

  if (queryChanged || itemsChanged) {
    // A new job invalidates any merged results we got so far.
    published = false;
    mergeState.reset();

    // Prepare results. Start from scratch with a new item vector and "timestamp" the results.
    auto& out = output.writeBuffer();
    out.mItemsTick = job.mItems.size();
    out.mQueryTick = job.mQueryTick;
    out.mQuery = job.mQuery;
    out.mItems.clear();

    // If there is no active query, publish empty results.
    if (!job.mQuery || job.mQuery->empty()) {
      publish();
      goto wait;
    }

    ASSERT(job.mQueue);
    auto& queue = *job.mQueue;
    const auto& query = *job.mQuery;
    out.mItems.reserve(job.mItems.size());
    for (;;) {
      // Reserve a chunk of items.
      //
      // We're not splitting the work evenly upfront, because some threads can have higher
      // workloads and take more time in total to process all items. The easiest way to work
      // around this problem is to just get items in fixed sized chunks in a loop. We're paying
      // with L1 cache misses here, but in the end it's insignificant compared to the disaster
      // that calculating the score is, so it's still an overall improvement for some cases.
      //
      // Right now reserve is an atomic fetch-add, that can set the shared counter to an out of
      // bounds value. This is fine for the time being, but if we want to reuse already calculated
      // items, it will have to be changed to a CAS loop, to guarantee we were within the previous
      // boundaries when new items are appended.
      const size_t start = std::min(queue.take(kChunkSize), job.mItems.size());
      const size_t end = std::min(start + kChunkSize, job.mItems.size());
      if (start >= end)
        break;

      auto selectScore = [](size_t needleSize, auto&& fn) {
        switch (needleSize) {
        default:
          fn(fzy::score);
          break;
        case 1:
          fn(fzy::score1);
          break;
#if defined(FZX_SSE2)
        case 2: case 3: case 4:
          fn(fzy::scoreSSE<4>);
          break;
        case 5: case 6: case 7: case 8:
          fn(fzy::scoreSSE<8>);
          break;
        case 9: case 10: case 11: case 12:
          fn(fzy::scoreSSE<12>);
          break;
        case 13: case 14: case 15: case 16:
          fn(fzy::scoreSSE<16>);
          break;
#elif defined(FZX_NEON)
        case 2: case 3: case 4:
          fn(fzy::scoreNeon<4>);
          break;
        case 5: case 6: case 7: case 8:
          fn(fzy::scoreNeon<8>);
          break;
        case 9: case 10: case 11: case 12:
          fn(fzy::scoreNeon<12>);
          break;
        case 13: case 14: case 15: case 16:
          fn(fzy::scoreNeon<16>);
          break;
#endif
        }
      };

      // if (query.mMatch.empty() && query.mScore.size() == 1) {
      //   // Single fuzzy search fast path
      //   const auto& needle = query.mScore.front().mText;
      //   selectScore(needle.size(), [&](auto&& fn) {
      //     for (size_t i = start; i < end; ++i) {
      //       auto item = job.mItems.at(i);
      //       if (matchFuzzy(needle, item))
      //         out.mItems.push_back({ static_cast<uint32_t>(i), fn(needle, item) });
      //     }
      //   });
      // } else {
        for (size_t i = start; i < end; ++i) {
          auto item = job.mItems.at(i);
          if (!query.match(item))
            continue;
          fzy::Score score = 0; // TODO: calculate score from exact/begin/end/substr
          for (const auto& needle : query.mScore) {
            selectScore(needle.mText.size(), [&](auto&& fn) {
              score += fn(needle.mText, item);
            });
          }
          out.mItems.push_back({ static_cast<uint32_t>(i), score });
        }
      // }

      // Ignore kMerge events from other workers, we don't care about
      // them at this stage, as we don't even have out own results yet.
      if (ev = events.get(); ev & (kStop | kJob))
        goto start;
    }

    // Sort the local batch of items.
    std::sort(out.mItems.begin(), out.mItems.end());
  }

  // Merge results from other threads.
  if (!mergeState.done()) {
    auto& out = output.writeBuffer();
    for (uint8_t i = 0; i < mergeState.size(); ++i) {
      // Already got results from this worker, try the next one.
      if (mergeState.contains(i))
        continue;

      // Load the results from this worker.
      const uint8_t id = mergeState.at(i);
      mPool->mWorkers[id]->mOutput.load();
      const auto& cres = mPool->mWorkers[id]->mOutput.readBuffer();

      // We've received results with a newer timestamp than ours, so that has to mean
      // there is a new job that we weren't aware about. Properly wait for new events
      // and process all of them, just in case.
      if (cres.newerThan(out))
        goto wait;

      // No agreement on the timestamp, these results are from some older job, and we
      // don't want to merge unrelated results. Skip this worker for now.
      if (!cres.sameTick(out))
        continue;

      // We agree on the timestamp, results from the child can be merged with our own.
      if (!cres.mItems.empty()) { // Avoid unnecessary copies
        merge2(tmp, out.mItems, cres.mItems); // TODO: merge in place?
        swap(tmp, out.mItems);
      }

      mergeState.set(i); // Mark this worker as merged.
    }

    // Not all results have been merged yet, we're still waiting for someone.
    if (!mergeState.done())
      goto wait;
  }

  // Publish results.
  publish();
  goto wait;
} catch (const std::exception& e) {
  std::strncpy(mErrorMsg, e.what(), std::size(mErrorMsg));
  mError.store(true);
  try {
    // TODO: Send a different byte over the pipe to communicate a critical error?
    mPool->mCallback(mPool->mUserData);
  } catch (...) {
    // Nothing else we can possibly do to communicate an error.
  }
}

} // namespace fzx
