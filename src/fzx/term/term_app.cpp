#include "fzx/term/term_app.hpp"

#include <cstdio>
#include <string_view>
#include <iostream>
#include <array>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

#include "fzx/term/key.hpp"
#include "fzx/macros.hpp"

using namespace std::string_view_literals;

namespace fzx {

static constexpr auto kMaxInputBufferSize = 0x40000; // limit buffer size to 256kb max

void TermApp::processInput()
{
  auto len = read(mInput.fd(), mInputBuffer.data(), mInputBuffer.size());
  if (len > 0) {
    if (mFzx.scanFeed({ mInputBuffer.data(), size_t(len) }) > 0)
      mFzx.commitItems();
    // resize the buffer if data can be read in bigger chunks
    if (mInputBuffer.size() == size_t(len) && mInputBuffer.size() < kMaxInputBufferSize)
      mInputBuffer.resize(mInputBuffer.size() * 2);
    redraw();
  } else if (len == 0) {
    mInput.close();
    if (mFzx.scanEnd())
      mFzx.commitItems();
    mInputBuffer.clear();
    mInputBuffer.shrink_to_fit();
    redraw();
  } else if (len == -1) {
    perror("read");
  }
}

void TermApp::processTTY()
{
  bool updateQuery = false;
  while (auto key = mTTY.read()) {
    ASSERT(key.has_value()); // checked in the while condition, but clang-tidy still complains
    if (mLine.handle(*key)) {
      updateQuery = true;
      continue;
    }
    if (*key == kEnter || *key == kCtrlU) {
      mLine.clear();
      updateQuery = true;
    } else if (*key == kCtrlC) {
      quit();
      return;
    }
  }
  if (updateQuery) {
    mFzx.setQuery(mLine.line());
  }
  redraw();
}

void TermApp::processWakeup()
{
  if (mFzx.loadResults())
    redraw();
}

void TermApp::processResize()
{
  mTTY.updateSize();
  redraw();
}

void TermApp::redraw()
{
  mTTY.put("\x1B[2J\x1B[H"sv);

  ASSERT(mTTY.height() > 3);
  auto maxHeight = mTTY.height() - 2;
  for (uint16_t i = 0; i < maxHeight && i < mFzx.resultsSize(); ++i) {
    auto s = mFzx.getResult(i).mLine;
    s = { s.data(), std::min(s.size(), size_t { 80 }) };
    mTTY.put("\x1B[{};0H  {}", maxHeight - i, s);
  }

  mTTY.put("\x1B[{};0H{}/{}", mTTY.height() - 1, mFzx.resultsSize(), mFzx.itemsSize());
  mTTY.put("\x1B[{};0H> {}", mTTY.height(), mLine.line());
  mTTY.flush();
}

void TermApp::quit()
{
  mQuit = true;
}

} // namespace fzx
