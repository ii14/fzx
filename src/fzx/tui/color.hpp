#pragma once
#include "fzx/macros.hpp"
#include <cstdint>
#include <variant>
#include <string_view>
#include <fmt/core.h>

namespace fzx {
constexpr static uint8_t parseNibble(char c)
{
  ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else {
    return c - 'a' + 0xa;
  }
}

constexpr static uint8_t parseByte(std::string_view byte)
{
  ASSERT(byte.size() == 2);
  return parseNibble(byte[0]) * 16 + parseNibble(byte[1]);
}

enum TermColorCode : uint8_t {
  Black = 0,
  Red = 1,
  Green = 2,
  Yellow = 3,
  Blue = 4,
  Purple = 5,
  Cyan = 6,
  White = 7,
  Default = 9,
};

struct TermColor
{
  TermColorCode mCode;
  bool mBright;
  constexpr TermColor(TermColorCode code, bool bright) : mCode(code), mBright(bright) { }
  constexpr TermColor(TermColorCode code) : mCode(code), mBright(false) { }
};

struct TrueColor
{
  uint8_t mRed, mGreen, mBlue;
  constexpr TrueColor(std::string_view hexString)
    : mRed(parseByte(hexString.substr(1, 2)))
    , mGreen(parseByte(hexString.substr(3, 2)))
    , mBlue(parseByte(hexString.substr(5, 2)))
  {
    ASSERT(hexString.size() == 7);
    ASSERT(hexString.front() == '#');
  }
  constexpr TrueColor(uint8_t red, uint8_t green, uint8_t blue)
    : mRed(red), mGreen(green), mBlue(blue)
  {
  }
};

class Color
{
public:
  std::variant<TrueColor, TermColor> mInner;
  constexpr Color(TermColor color) : mInner(color) { }
  constexpr Color(TermColorCode color, bool bright) : mInner(TermColor(color, bright)) { }
  constexpr Color(TermColorCode color) : mInner(TermColor(color)) { }
  constexpr Color(TrueColor color) : mInner(color) { }
  constexpr Color(uint8_t red, uint8_t green, uint8_t blue)
    : mInner(TrueColor(red, green, blue)) { }
  constexpr Color(std::string_view hexString) : mInner(TrueColor(hexString)) { }
};

struct Theme
{
  Color mDefaultFg, mDefaultBg, mPromptFg, mPromptBg, mMatchFg, mCursorFg, mCursorBg;
  constexpr Theme()
    : mDefaultFg(Color("#ffffff"))
    , mDefaultBg(Color("#000000"))
    , mPromptFg(Color("#b6a0ff"))
    , mPromptBg(Color("#2f0c3f"))
    , mMatchFg(Color("#00bcff"))
    , mCursorFg(Color("#ffffff"))
    , mCursorBg(Color("#2f447f"))
  {
  }
};
} // namespace fzx
