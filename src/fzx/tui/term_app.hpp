#pragma once

#include "fzx/fzx.hpp"
#include "fzx/tui/input.hpp"
#include "fzx/tui/line_editor.hpp"
#include "fzx/tui/tty.hpp"
#include "fzx/helper/eventfd.hpp"
#include "fzx/helper/line_scanner.hpp"

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
  LineScanner mLineScanner;

  Input mInput;
  TTY mTTY;

  LineEditor mLine;

  std::vector<char> mInputBuffer;
  uint32_t mScanPos { 0 };

  bool mQuit { false };
};

} // namespace fzx
