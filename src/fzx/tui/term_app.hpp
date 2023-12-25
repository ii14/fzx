#pragma once

#include "fzx/fzx.hpp"
#include "fzx/tui/input.hpp"
#include "fzx/tui/line_editor.hpp"
#include "fzx/tui/tty.hpp"
#include "fzx/helper/eventfd.hpp"
#include "fzx/helper/line_scanner.hpp"
#include <set>

namespace fzx {
enum class Status { Running, ExitSuccess, ExitFailure };
// all of this sucks atm, it's just to get things going
struct TermApp
{
  TermApp() { mInputBuffer.resize(1024); }

  void processInput();
  void processTTY();
  void processResize();
  void processWakeup();
  void redraw();
  void quit(bool success);
  void printSelection();
  std::string_view currentItem() const;
  [[nodiscard]] bool running() const noexcept { return mStatus == Status::Running; }

  EventFd mEventFd;
  Fzx mFzx;
  LineScanner mLineScanner;

  Input mInput;
  TTY mTTY;

  LineEditor mLine;

  std::vector<char> mInputBuffer;
  uint32_t mScanPos { 0 };

  Status mStatus { Status::Running };
  std::set<uint32_t> mSelection = {};

private:
  size_t mCursor = 0;
};

} // namespace fzx
