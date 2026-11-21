#pragma once
#include <Arduino.h>

#ifndef LGFX_USE_V1
#define LGFX_USE_V1
#endif
#include <LovyanGFX.hpp>

// Album art for the screen. Covers are JPEG/PNG images served by the Sonos
// player on the LAN. Loading is staged so the screen never waits on it:
//
//   select()  instant: a prefetched cover, or the album's remembered colours
//   step()    1) download + 1/8-scale decode -> blurry preview + palette (~0.45 s)
//             2) full decode -> sharp cover (~0.3 s more)
//   prefetch() loads the next queue track's cover while idle
//   The last ~16 covers stay decoded in PSRAM, so going back is instant too.
//
// All functions are for the action worker task only (network + decoding).
namespace art {

struct Palette {
  uint32_t base;    // dark background tint (RGB888)
  uint32_t accent;  // light tint for icons / volume bar
  uint32_t soft;    // secondary text
};

enum class Stage : uint8_t {
  None,     // no cover for this item
  Pending,  // not loaded yet: draw a gradient from palette()
  Preview,  // preview() holds a tiny version: draw it scaled up (soft focus)
  Full,     // full() holds the sharp cover, display width wide
  Failed,   // couldn't load: treat like None
};

void begin();  // loads the album colour cache
Palette defaultPalette();

// Make `url` the current cover. albumKey ("artist|album") finds remembered colours.
void select(const String &url, const String &albumKey);
// Do the next loading stage for the current cover. True if the stage changed.
bool step();
// Download and fully decode `url` into a free (or least recently used) cache slot (blocking, ~0.75-1.6 s).
void prefetch(const String &url);

Stage stage();
Palette palette();
lgfx::LGFX_Sprite &preview();
lgfx::LGFX_Sprite &full();

}  // namespace art
