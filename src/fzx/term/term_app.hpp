#pragma once

#include "fzx/fzx.hpp"
#include "fzx/term/input.hpp"
#include "fzx/term/line_editor.hpp"
#include "fzx/term/tty.hpp"
#include "fzx/eventfd.hpp"

namespace fzx {

// all of this sucks atm, it's just to get things going
struct TermApp
{
  TermApp()
  {
    mInputBuffer.resize(1024);
  }

  void processInput();
  void processTTY();
  void processResize();
  void processWakeup();
  void redraw();
  void quit();
  [[nodiscard]] bool running() const noexcept { return !mQuit; }

  EventFd mEventFd;
  Fzx mFzx;

  Input mInput;
  TTY mTTY;

  LineEditor mLine;

  std::vector<char> mInputBuffer;
  uint32_t mScanPos { 0 };

  bool mQuit { false };
};

} // namespace fzx
