// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#include <algorithm>
#include <cstring>
#include <string_view>
#include <vector>

#include "fzx/macros.hpp"

namespace fzx {

struct LineScanner
{
  template <typename Push>
  uint32_t feed(std::string_view str, Push&& push, char ch = '\n')
  {
    uint32_t count = 0;
    const auto* it = str.begin();
    while (true) {
      const auto* const nl = std::find(it, str.end(), ch);
      const auto len = std::distance(it, nl);
      ASSUME(len >= 0);
      if (nl == str.end()) {
        if (len != 0) {
          const auto size = mBuf.size();
          mBuf.resize(size + len);
          std::memcpy(mBuf.data() + size, it, len);
        }
        return count;
      } else if (len == 0) {
        if (!mBuf.empty()) {
          push({ mBuf.data(), mBuf.size() });
          mBuf.clear();
          ++count;
        }
      } else if (mBuf.empty()) {
        push({ it, static_cast<size_t>(len) });
        ++count;
      } else {
        const auto size = mBuf.size();
        mBuf.resize(size + len);
        std::memcpy(mBuf.data() + size, it, len);
        push({ mBuf.data(), mBuf.size() });
        mBuf.clear();
        ++count;
      }
      it += len + 1;
    }
  }

  template <typename Push>
  bool finalize(Push&& push)
  {
    const bool empty = mBuf.empty();
    if (!empty)
      push({ mBuf.data(), mBuf.size() });
    mBuf.clear();
    mBuf.shrink_to_fit();
    return !empty;
  }

private:
  std::vector<char> mBuf;
};

} // namespace fzx
