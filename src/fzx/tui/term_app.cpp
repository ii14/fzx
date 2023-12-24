#include "fzx/tui/term_app.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <iostream>
#include <array>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

#include "fzx/tui/key.hpp"
#include "fzx/macros.hpp"

using namespace std::string_view_literals;

namespace fzx {

static constexpr auto kMaxInputBufferSize = 0x40000; // limit buffer size to 256kb max

void TermApp::processInput()
{
  auto push = [this](std::string_view item) { mFzx.pushItem(item); };
  try {
    auto len = read(mInput.fd(), mInputBuffer.data(), mInputBuffer.size());
    if (len > 0) {
      if (mLineScanner.feed({ mInputBuffer.data(), size_t(len) }, push) > 0)
        mFzx.commit();
      // resize the buffer if data can be read in bigger chunks
      if (mInputBuffer.size() == size_t(len) && mInputBuffer.size() < kMaxInputBufferSize)
        mInputBuffer.resize(mInputBuffer.size() * 2);
      redraw();
      return;
    } else if (len == 0) {
      mInput.close();
      if (mLineScanner.finalize(push))
        mFzx.commit();
      mInputBuffer.clear();
      mInputBuffer.shrink_to_fit();
      redraw();
    } else if (len == -1) {
      perror("read");
      return;
    }
  } catch (...) {
    // catch out of memory exceptions
    mInput.close();
    if (mLineScanner.finalize(push))
      mFzx.commit();
    mInputBuffer.clear();
    mInputBuffer.shrink_to_fit();
    redraw();
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
    if (*key == kEnter) {
      quit(true);
      return;
    } else if (*key == kCtrlU) {
      mLine.clear();
      updateQuery = true;
    } else if (*key == kCtrlC) {
      quit(false);
      return;
    } else if (*key == kCtrlP) {
      mCursor++;
    } else if (*key == kCtrlN) {
      if (mCursor > 0) {
        mCursor--;
      }
    } else if (*key == kTab) {
      if (mCursor < mFzx.resultsSize()) {
        const uint32_t index = mFzx.getResult(mCursor).mIndex;
        if (mSelection.find(index) != mSelection.end()) {
          mSelection.erase(index);
        } else {
          mSelection.insert(index);
        }
        mCursor++;
      }
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
  if (mTTY.height() < 4 || mTTY.width() < 4)
    return;

  int maxHeight = mTTY.height() - 2;
  int itemWidth = mTTY.width() - 2;
  size_t items = mFzx.resultsSize();
  mCursor = std::clamp(mCursor, (size_t)0, items - 1);

  Positions positions;
  constexpr auto kInvalid = std::numeric_limits<size_t>::max();
  std::string_view query = mFzx.query();

  for (int i = 0; i < maxHeight; ++i) {
    mTTY.put("\x1B[{};0H\x1B[K", maxHeight - i);
    if (static_cast<size_t>(i) < items) {
      const auto result = mFzx.getResult(i);
      std::string_view item = result.mLine;
      if (mCursor == static_cast<size_t>(i)) {
        mTTY.put("► ", maxHeight - i);
      } else {
        mTTY.put("  ", maxHeight - i);
      }
      if (mSelection.find(result.mIndex) != mSelection.end()) {
        mTTY.put("•", maxHeight - i);
      } else {
        mTTY.put(" ", maxHeight - i);
      }
      std::fill(positions.begin(), positions.end(), kInvalid);
      matchPositions(query, item, &positions);

      bool highlighted = false;
      std::string_view sub = item.substr(0, itemWidth);
      for (size_t i = 0, p = 0; i < sub.size(); ++i) {
        if (p < positions.size() && positions[p] == i) {
          ++p;
          if (!highlighted) {
            highlighted = true;
            mTTY.put("\x1B[33m"sv);
          }
        } else {
          if (highlighted) {
            highlighted = false;
            mTTY.put("\x1B[0m"sv);
          }
        }
        mTTY.put(sub[i]);
      }
      if (highlighted) {
        mTTY.put("\x1B[0m"sv);
      }
    }
  }

  mTTY.put("\x1B[{};0H\x1B[K{}/{}", mTTY.height() - 1, mFzx.resultsSize(), mFzx.itemsSize());
  mTTY.put("\x1B[{};0H\x1B[K> {}", mTTY.height(), mLine.line());

  mTTY.flush();
}

void TermApp::quit(bool success)
{
  if (success) {
    mStatus = Status::ExitSuccess;
  } else {
    mStatus = Status::ExitFailure;
  }
}

void TermApp::printSelection()
{
  for (const auto& index : mSelection) {
    std::cout << mFzx.getItem(index) << std::endl;
  }
}

std::string_view TermApp::currentItem() const {
  if (mCursor < mFzx.resultsSize())
    return mFzx.getResult(mCursor).mLine;
  return {};
}


} // namespace fzx
