// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

// TODO: windows support
#if !defined(_WIN32)

# include <atomic>
# include <mutex>
# include <string>

namespace fzx {

/// Notify external event loops about new activity.
struct EventFd
{
  EventFd() = default;
  ~EventFd() noexcept { close(); }

  EventFd(EventFd&&) = delete;
  EventFd& operator=(EventFd&&) = delete;
  EventFd(const EventFd&) = delete;
  EventFd& operator=(const EventFd&) = delete;

  /// @return Error message or empty string on success.
  std::string open();
  [[nodiscard]] bool isOpen() const noexcept { return mPipe[0] != -1; }
  void close() noexcept;

  /// Get the event file descriptor.
  ///
  /// External event loops should listen for read events on
  /// this file descriptor to get notified about new activity.
  [[nodiscard]] int fd() const noexcept { return mPipe[0]; }
  /// After you've been notified about new activity,
  /// you should call this before processing any data.
  void consume() noexcept;

  /// Notify about new activity. Safe to call from multiple threads.
  void notify();

private:
  int mPipe[2] { -1, -1 };
  std::atomic<bool> mActive { false };
  mutable std::mutex mMutex;
};

} // namespace fzx

#endif
