#pragma once

#include <string>

namespace fzx {

struct LineEditor
{
  bool handle(uint8_t key);
  void clear() noexcept { mLine.clear(); }
  [[nodiscard]] const std::string& line() const noexcept { return mLine; }

private:
  size_t mCursor { 0 };
  std::string mLine;
};

} // namespace fzx
