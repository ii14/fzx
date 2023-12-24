#pragma once
#include "fzx/macros.hpp"
#include <cstdint>
#include <sstream>
struct Color {
  uint8_t mRed, mGreen, mBlue;
  Color(const std::string& hexstring);
};

struct Theme {
  Color mPromptFg, mPromptBg, mMatchFg, mMatchBg, mSelectionFg, mSelectionBg;
};
