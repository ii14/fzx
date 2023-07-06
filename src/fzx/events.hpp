#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "fzx/util.hpp"

namespace fzx {

// for benchmarking
// #define FZX_EVENTS_NO_ATOMICS

struct Events
{
  enum Flags : uint32_t {
    kNone = 0,

    kStopEvent = 1U << 0,
    kItemsEvent = 1U << 1,
    kQueryEvent = 1U << 2,

    kEventMask = 0x7FFFFFFF,
    kWaiting = 0x80000000,
  };

  /// Check this once in a while if we haven't got any update.
  uint32_t get() noexcept
  {
    return mState.exchange(kNone);
  }

  /// Once we truly have nothing else to do, try to put the thread to sleep.
  uint32_t wait() noexcept
  {
#ifndef FZX_EVENTS_NO_ATOMICS
    // Signal to other threads with kWaiting flag that
    // we're out of work to do, and we might go to sleep.
    if ((mState.fetch_or(kWaiting) & kEventMask) != kNone)
      // If we received some event, consume it while also unsetting kWaiting flag.
      // Maybe kWaiting doesn't necessarily have to be always set, but then I'm not
      // sure if without it you can guarantee the correctness with post function.
      // I haven't thought about it too much though, so maybe you can. TODO: do the thinking
      return mState.exchange(kNone);
#else
    if (uint32_t state = mState.exchange(kNone); state != kNone)
      return state;
#endif

    {
      std::unique_lock lock { mMutex };
      mCv.wait(lock, [&] {
        // TODO: would it be any better to wait until mState != kWaiting?
        //       could that simplify something in the post function?
        return (mState.load() & kEventMask) != kNone;
      });
    }

    return mState.exchange(kNone);
  }

  /// Post event.
  void post(uint32_t flags) noexcept
  {
    DEBUG_ASSERT((flags & kWaiting) == 0); // Trying to set a private flag
    DEBUG_ASSERT((flags & kEventMask) != 0); // No flags set

    uint32_t state = mState.fetch_or(flags) | flags; // Add new flags
#ifndef FZX_EVENTS_NO_ATOMICS
    if (!(state & kWaiting)) // Thread is not waiting, don't have to wake it up
      return;

    // Try to unset kWaiting flag, so no one else has to post the thread.
    // The thread might've already consumed the flags and removed kWaiting
    // flag in its fast path, or some other thread tries to do this right
    // now, so return early if that happens.
    while (!mState.compare_exchange_weak(state, state & kEventMask))
      if ((state & kEventMask) == kNone || !(state & kWaiting))
        return;
#endif

    {
      // std::condition_variable::notify_* requires locking a
      // mutex first, even if it doesn't actually do anything.
      std::unique_lock lock { mMutex };
    }
    mCv.notify_one();
  }

  // std::mutex::lock and std::mutex::unlock are not noexcept, but none of the exceptions
  // that they can throw should ever happen. We need noexcept because the post function
  // in particular can be called in a destructor.

private:
  std::atomic<uint32_t> mState { kNone };
  std::mutex mMutex;
  std::condition_variable mCv;
};

} // namespace fzx
