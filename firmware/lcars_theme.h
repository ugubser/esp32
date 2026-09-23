#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace lcars {
// Semantic LCARS colour slots. Text on filled buttons is always BLACK.
struct Theme {
  const char *name;
  uint32_t primary;    // frame, active selection, lights that are on
  uint32_t secondary;  // menu buttons, section titles
  uint32_t tertiary;   // navigation, secondary text
  uint32_t text;       // body text on black
  uint32_t off_bg;     // inactive tiles, tracks and dividers
  uint32_t alert;      // transit delays and imminent departures
  uint32_t flash;      // brief highlight while a button is pressed
};
inline constexpr uint32_t BLACK=0x060608;
inline constexpr std::array<Theme,4> THEMES{{
  {"CLASSIC",      0xFFB780,0xC2A5E5,0x99B9EE,0xFFE3C6,0x241D2D,0xFFD36A,0xFFF4E8},
  {"NEMESIS BLUE", 0x6FA6F2,0x5584D6,0xA9CCF7,0xDCEBFF,0x141E36,0xFFC857,0xEEF6FF},
  {"RED ALERT",    0xE8553F,0xFF8A65,0xF2B8A0,0xFFDCD0,0x2C1414,0xFFD36A,0xFFEDE6},
  {"VOYAGER",      0xE1A15B,0xC9825A,0xDCC6A2,0xFFEBD0,0x2A2118,0xFF8C5A,0xFFF4E4},
}};
// Restored preferences from an older or corrupted flash entry select CLASSIC.
inline size_t theme_index(int stored) {
  return stored>=0 && size_t(stored)<THEMES.size() ? size_t(stored) : 0;
}
}  // namespace lcars
