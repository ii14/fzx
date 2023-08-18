// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <thread>
#include <utility>

#include "fzx/util.hpp"

namespace fzx {

/// std::thread but join on destruction. Partial std::jthread backport
struct Thread
{
  Thread() noexcept = default;

  Thread(Thread&& b) noexcept : mThread(std::move(b.mThread)) { }

  template <typename Fn, typename... Args>
  explicit Thread(Fn&& fn, Args&&... args)
    : mThread(std::forward<Fn>(fn), std::forward<Args>(args)...)
  {
  }

  Thread& operator=(Thread&& b) noexcept
  {
    if (this == &b)
      return *this;
    if (mThread.joinable())
      mThread.join();
    mThread = std::move(b.mThread);
    return *this;
  }

  ~Thread() noexcept
  {
    if (mThread.joinable())
      mThread.join();
  }

  void swap(Thread& b) noexcept { fzx::swap(mThread, b.mThread); }

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  [[nodiscard]] auto joinable() const noexcept { return mThread.joinable(); }
  [[nodiscard]] auto get_id() const noexcept { return mThread.get_id(); }

  void join() { mThread.join(); }
  void detach() { mThread.detach(); }

  static void pin(int cpu);

private:
  std::thread mThread;
};

inline void swap(Thread& a, Thread& b) noexcept
{
  a.swap(b);
}

} // namespace fzx
