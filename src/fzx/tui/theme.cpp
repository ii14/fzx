#include "theme.hpp"

Color::Color(const std::string &hexstring) {
  ASSERT(hexstring.front() == '#');
  ASSERT(hexstring.size() == 7);
  std::istringstream(hexstring.substr(1, 2)) >> std::hex >> mRed;
  std::istringstream(hexstring.substr(3, 2)) >> std::hex >> mGreen;
  std::istringstream(hexstring.substr(5, 2)) >> std::hex >> mBlue;
}
