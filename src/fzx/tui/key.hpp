#pragma once

#include <cstdint>

namespace fzx {

enum Key : uint8_t {
  kEnter = 0x0D,
  kEscape = 0x1B,
  kReturn = 0x1B,
  kNewLine = 0x0A,
  kBackspace = 0x7F,
  kTab = 0x09,

  kCtrlSpace = 0x00,
  kCtrlAt = 0x00,
  kCtrlLBracket = 0x1B,
  kCtrlBackslash = 0x1C,
  kCtrlRBracket = 0x1D,
  kCtrlCarot = 0x1E,
  kCtrlUnderscore = 0x1F,

  kCtrlA = 0x01,
  kCtrlB = 0x02,
  kCtrlC = 0x03,
  kCtrlD = 0x04,
  kCtrlE = 0x05,
  kCtrlF = 0x06,
  kCtrlG = 0x07,
  kCtrlH = 0x08,
  kCtrlI = 0x09,
  kCtrlJ = 0x0A,
  kCtrlK = 0x0B,
  kCtrlL = 0x0C,
  kCtrlM = 0x0D,
  kCtrlN = 0x0E,
  kCtrlO = 0x0F,
  kCtrlP = 0x10,
  kCtrlQ = 0x11,
  kCtrlR = 0x12,
  kCtrlS = 0x13,
  kCtrlT = 0x14,
  kCtrlU = 0x15,
  kCtrlV = 0x16,
  kCtrlW = 0x17,
  kCtrlX = 0x18,
  kCtrlY = 0x19,
  kCtrlZ = 0x1A,
};

} // namespace fzx
