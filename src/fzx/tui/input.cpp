#include "fzx/tui/input.hpp"

#include <cstdio>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

namespace fzx {

bool Input::open() noexcept
{
  if (isatty(STDIN_FILENO)) {
    fprintf(stderr, "no input\r\n");
    return false;
  }
  mFd = STDIN_FILENO;
  return true;
}

void Input::close() noexcept
{
  mFd = -1;
}

} // namespace fzx
