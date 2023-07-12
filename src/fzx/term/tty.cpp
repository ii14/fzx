#include "fzx/term/tty.hpp"

#include <cstdio>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
}

#include "fzx/macros.hpp"

using namespace std::string_view_literals;

namespace fzx {

static struct termios gSavedAttrs {};
static bool gActive { false };

TTY::~TTY() noexcept
{
  close();
}

TTY::TTY(TTY&& b) noexcept
  : mFd(b.mFd)
{
  b.mFd = kInvalidFd;
}

TTY& TTY::operator=(TTY&& b) noexcept
{
  close();
  mFd = b.mFd;
  b.mFd = kInvalidFd;
  return *this;
}

bool TTY::open() noexcept
{
  ASSERT(!isOpen());
  ASSERT(!gActive);

  const int fd = ::open("/dev/tty", O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    perror("open(\"/dev/tty\")");
    return false;
  }

  if (!isatty(fd)) {
    fprintf(stderr, "not a tty\n");
    ::close(fd);
    return false;
  }

  if (tcgetattr(fd, &gSavedAttrs)) {
    perror("tcgetattr");
    ::close(fd);
    return false;
  }

  struct termios attrs = gSavedAttrs;
  attrs.c_iflag &= ~(ICRNL | IXON);
  attrs.c_lflag &= ~(ICANON | ECHO | ISIG);
  if (tcsetattr(fd, TCSANOW, &attrs) != 0) {
    perror("tcsetattr");
    ::close(fd);
    return false;
  }

  mFd = fd;
  gActive = true;

  updateSize();
  put("\x1B[?1049h\x1B[2J\x1B[H"sv);
  flush();
  return true;
}

void TTY::close() noexcept
{
  if (!isOpen())
    return;
  put("\x1B[?1049l"sv);
  flush();
  tcsetattr(mFd, TCSANOW, &gSavedAttrs);
  ::close(mFd);
  mFd = kInvalidFd;
  gActive = false;
}

void TTY::updateSize() noexcept
{
  struct winsize ws {};
  ioctl(mFd, TIOCGWINSZ, &ws);
  mWidth = ws.ws_col;
  mHeight = ws.ws_row;
}

std::optional<uint8_t> TTY::read() const noexcept
{
  uint8_t ch = 0;
  auto res = ::read(mFd, &ch, 1);
  if (res == 1)
    return ch;
  if (res == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    perror("read"); // TODO: use exceptions or whatever
  return std::nullopt;
}

void TTY::put(char c)
{
  mBuffer.push_back(c);
}

void TTY::put(const char* s)
{
  if (s != nullptr)
    put(std::string_view { s });
}

void TTY::put(const std::string& s)
{
  if (!s.empty())
    put(std::string_view { s });
}

void TTY::put(std::string_view s)
{
  if (s.empty())
    return;
  const auto size = mBuffer.size();
  mBuffer.resize(size + s.size());
  std::memcpy(mBuffer.data() + size, s.data(), s.size());
}

void TTY::flush() noexcept
{
  if (mBuffer.empty() || !isOpen())
    return;
  UNUSED(::write(mFd, mBuffer.data(), mBuffer.size()));
  mBuffer.clear();
}

} // namespace fzx
