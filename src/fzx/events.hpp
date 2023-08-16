#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace fzx {

struct Events
{
  /// Check this once in a while if we haven't got any update.
  uint32_t get() noexcept;
  /// Once we truly have nothing else to do, try to put the current thread to sleep.
  uint32_t wait();

  /// Post event from other thread.
  /// The highest bit is reserved.
  void post(uint32_t flags);

private:
  std::atomic<uint32_t> mState { 0 };
  std::mutex mMutex;
  std::condition_variable mCv;
};

} // namespace fzx
