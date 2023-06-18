#include "fzx/term/term_app.hpp"

#include <cstdio>
#include <string_view>
#include <iostream>
#include <array>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

#include "fzx/term/key.hpp"

using namespace std::string_view_literals;

namespace fzx {

static constexpr auto kMaxInputSize = std::numeric_limits<uint32_t>::max();
static constexpr auto kMaxInputBufferSize = 0x20000; // limit buffer size to 128kb max

void TermApp::processInput()
{
  // TODO: last line is not included if there is no newline character
  auto len = read(mInput.fd(), mInputBuffer.data(), mInputBuffer.size());
  if (len > 0) {
    const size_t pos = mBuffer.size();

    // resize the buffer if data can be read in bigger chunks
    if (mInputBuffer.size() == size_t(len) && mInputBuffer.size() < kMaxInputBufferSize)
      mInputBuffer.resize(mInputBuffer.size() * 2);

    // 4gb of input data max, offsets are stored as uint32s
    if (pos + len >= kMaxInputSize) {
      len -= ssize_t(kMaxInputSize - pos);
      mInput.close();
    }

    // TODO: ItemList stores the entire thing already, this unnecessarily doubles memory usage.
    // a small dynamic buffer should be used just to store single line that is currently parsed.
    mBuffer.resize(pos + len);
    std::memcpy(mBuffer.data() + pos, mInputBuffer.data(), len);

    for (;;) {
      const auto it = std::find(mBuffer.begin() + mScanPos, mBuffer.end(), '\n');
      if (it == mBuffer.end())
        break;
      const auto size = std::distance(mBuffer.begin() + mScanPos, it);
      if (size > 0) {
        // TODO: indices should be stored and known for empty items too.
        // the user can associate line index with some data outside of this program.
        mFzx.pushItem({ mBuffer.data() + mScanPos, size_t(size) });
      }
      mScanPos += size + 1;
    }

    mFzx.commitItems();

    redraw();
  } else if (len == 0) {
    mInput.close();
    redraw();
    // free the input buffer
    mInputBuffer.clear();
    mInputBuffer.shrink_to_fit();
    // this one too, but this one shouldn't really be a thing in the first place
    mBuffer.clear();
    mBuffer.shrink_to_fit();
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
  if (mLine.line().empty()) {
    for (uint16_t i = 0; i < maxHeight && i < mFzx.itemsSize(); ++i) {
      auto s = mFzx.getItem(i);
      s = { s.data(), std::min(s.size(), size_t { 80 }) };
      mTTY.put("\x1B[{};0H  {}", maxHeight - i, s);
    }
  } else {
    for (uint16_t i = 0; i < maxHeight && i < mFzx.resultsSize(); ++i) {
      auto s = mFzx.getResult(i).mLine;
      s = { s.data(), std::min(s.size(), size_t { 80 }) };
      mTTY.put("\x1B[{};0H  {}", maxHeight - i, s);
    }
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
