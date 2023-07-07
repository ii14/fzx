#pragma once

#include <vector>
#include <string_view>

namespace fzx {

struct LineScanner
{
  template <typename Push>
  uint32_t feed(std::string_view str, Push&& push, char ch = '\n');

  template <typename Push>
  bool finalize(Push&& push);

private:
  std::vector<char> mBuf;
};

} // namespace fzx
