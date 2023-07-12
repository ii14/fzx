#include "fzx/events.hpp"

#include "fzx/macros.hpp"

namespace fzx {

// for benchmarking
// #define FZX_EVENTS_NO_ATOMICS

namespace {

constexpr uint32_t kWaitFlag = 0x80000000;
constexpr uint32_t kEventMask = ~kWaitFlag;

} // namespace

// std::mutex::lock and std::mutex::unlock are not noexcept, but none of the exceptions
// that they can throw should ever happen. We need noexcept because the post function
// in particular can be called in a destructor.

// TODO: try to weaken memory ordering. Events::get in particular is called often, and
// acquire should be enough there.

uint32_t Events::get() noexcept
{
  return mState.exchange(0);
}

uint32_t Events::wait() noexcept
{
#ifndef FZX_EVENTS_NO_ATOMICS
  // Signal to other threads with kWaitFlag flag that
  // we're out of work to do, and we might go to sleep.
  if ((mState.fetch_or(kWaitFlag) & kEventMask) != 0)
    // If we received some event, consume it while also unsetting kWaitFlag flag.
    // Maybe kWaitFlag doesn't necessarily have to be always set, but then I'm not
    // sure if without it you can guarantee the correctness with post function.
    // I haven't thought about it too much though, so maybe you can. TODO: do the thinking
    return mState.exchange(0);
#else
  if (uint32_t state = mState.exchange(0); state != 0)
    return state;
#endif

  {
    std::unique_lock lock { mMutex };
    mCv.wait(lock, [&] {
      // TODO: would it be any better to wait until mState != kWaitFlag?
      //       could that simplify something in the post function?
      return (mState.load() & kEventMask) != 0;
    });
  }

  return mState.exchange(0);
}

void Events::post(uint32_t flags) noexcept
{
  DEBUG_ASSERT((flags & kWaitFlag) == 0); // Trying to set a private flag
  DEBUG_ASSERT(flags != 0); // No flags set

  uint32_t state = mState.fetch_or(flags) | flags; // Add new flags
#ifndef FZX_EVENTS_NO_ATOMICS
  if (!(state & kWaitFlag)) // Thread is not waiting, don't have to wake it up
    return;

  // Try to unset kWaitFlag flag, so no one else has to notify the thread.
  // The thread might've already consumed the flags and removed kWaitFlag
  // flag in its fast path, or some other thread tries to do this right
  // now, so return early if that happens.
  while (!mState.compare_exchange_weak(state, state & kEventMask))
    if ((state & kEventMask) == 0 || !(state & kWaitFlag))
      return;
#endif

  {
    // std::condition_variable::notify_* requires locking a
    // mutex first, even if it doesn't actually do anything.
    std::unique_lock lock { mMutex };
  }
  mCv.notify_one();
}

} // namespace fzx
