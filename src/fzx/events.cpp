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
  // Since only one thread can call get/wait, this could be improved by doing
  // `if (mState.load() == 0) return 0;` first to prevent unnecessary writes,
  // but it doesn't seem to matter for our application.
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
  ASSERT((flags & kWaitFlag) == 0); // Trying to set a private flag
  ASSERT(flags != 0); // No flags set
  // Add new flags. If the thread is in a waiting state,
  // let only the first person who got here through.
  if (mState.fetch_or(flags) != kWaitFlag)
    return;
  {
    // Synchronizes with mCv.wait in Events::wait
    std::unique_lock lock { mMutex };
  }
  mCv.notify_one();
}

} // namespace fzx
