#include "fzx/eventfd.hpp"

#include <stdexcept>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

#include "fzx/util.hpp"

namespace fzx {

std::string EventFd::open()
{
  using namespace std::string_literals;
  using namespace std::string_view_literals;

  if (mPipe[0] != -1 || mPipe[1] != -1)
    return "already open"s;

  if (pipe(mPipe) == -1) {
    const char* err = strerror(errno); // TODO: not thread safe
    return "pipe() failed: "s + (err != nullptr ? err : "(null)");
  }

  auto check = [this](int rc, std::string_view expr) -> std::string {
    if (rc != -1)
      return {};
    const int err = errno;
    ::close(mPipe[0]);
    ::close(mPipe[1]);
    mPipe[0] = -1;
    mPipe[1] = -1;
    const char* msg = strerror(err); // TODO: strerror is not thread safe
    return std::string{expr} + " failed: "s + (msg != nullptr ? msg : "(null)");
  };

#define CHECK_SYSCALL(expr) \
  if (auto msg = check(expr, #expr); !msg.empty()) \
    return msg
  CHECK_SYSCALL(fcntl(mPipe[0], F_SETFL, O_NONBLOCK));
  CHECK_SYSCALL(fcntl(mPipe[1], F_SETFL, O_NONBLOCK));
  CHECK_SYSCALL(fcntl(mPipe[0], F_SETFD, FD_CLOEXEC));
  CHECK_SYSCALL(fcntl(mPipe[1], F_SETFD, FD_CLOEXEC));
#undef CHECK_SYSCALL

  return {};
}

void EventFd::close() noexcept
{
  if (mPipe[0] != -1) {
    ::close(mPipe[0]);
    mPipe[0] = -1;
  }
  if (mPipe[1] != -1) {
    ::close(mPipe[1]);
    mPipe[1] = -1;
  }
}

void EventFd::consume() noexcept
{
  if (!isOpen() || !mActive.exchange(false, std::memory_order_seq_cst))
    return;
  char buf[32];
  UNUSED(::read(mPipe[0], buf, std::size(buf)));
}

void EventFd::notify() noexcept
{
  if (!isOpen() || mActive.exchange(true, std::memory_order_seq_cst))
    return;
  const char c = 0;
  UNUSED(::write(mPipe[1], &c, 1));
}

} // namespace fzx
