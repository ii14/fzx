// Licensed under LGPLv3 - see LICENSE file for details.

#include "fzx/events.hpp"

#include "fzx/macros.hpp"

namespace fzx {

namespace {

constexpr uint32_t kWaitFlag = 0x80000000;
constexpr uint32_t kEventMask = ~kWaitFlag;

} // namespace

uint32_t Events::get() noexcept
{
  return mState.exchange(0);
}

uint32_t Events::wait()
{
  std::unique_lock lock { mMutex };
  // Enter the waiting state with kWaitFlag. If there
  // are currently no events available, go to sleep.
  if (!(mState.fetch_or(kWaitFlag) & kEventMask))
    do mCv.wait(lock); while (!(mState.load() & kEventMask));
  // Exit the waiting state and consume events.
  return mState.exchange(0) & kEventMask;
}

void Events::post(uint32_t flags)
{
  DEBUG_ASSERT((flags & kWaitFlag) == 0); // Trying to set a private flag
  DEBUG_ASSERT(flags != 0); // No flags set
  // Add new flags. If the thread is in a waiting state,
  // let only the first person who got here through.
  if (mState.fetch_or(flags) != kWaitFlag)
    return;
  // std::condition_variable::notify_* requires locking a
  // mutex first, even if it doesn't actually do anything.
  std::unique_lock { mMutex }; // NOLINT(bugprone-unused-raii)
  mCv.notify_one();
}

} // namespace fzx
