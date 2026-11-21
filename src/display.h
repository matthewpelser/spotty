#pragma once
#include <Arduino.h>

#include <functional>

// The controller's screen: a 240x280 colour display (Waveshare ST7789 module).
// Every view is drawn into an off-screen framebuffer. The web page shows those
// exact pixels (GET /api/screen), and when DISPLAY_PANEL is enabled in config.h
// the same frame is pushed to the physical LCD.
//
// Layout "hero fade": album art across the top fading into a tint of its
// dominant colour, preset pill, title/artist, then room + volume.
//
// Drawing functions are for the action worker task only (they may download
// album art). copyFrame() is safe from any task.
namespace display {

constexpr int WIDTH = 240;
constexpr int HEIGHT = 280;

enum class Tone : uint8_t { Info, Busy, Error };

struct NowPlayingView {
  String group;   // "Kitchen", "All rooms", ...
  String badge;   // preset pill: preset name, "Random", or "" for none
  String title;
  String artist;
  String state;   // PLAYING, PAUSED_PLAYBACK, STOPPED, TRANSITIONING
  String artUrl;  // cover image URL, "" for none
  String albumKey;  // "artist|album": finds remembered cover colours
  int volume = -1;
};

void begin();

void message(const String &title, const String &detail, Tone tone = Tone::Info);
void nowPlaying(const NowPlayingView &view);
// Update just the volume on the now-playing screen (instant feedback for Vol±).
void volume(int vol);

// Frame as little-endian RGB565, row-major, WIDTH*HEIGHT*2 bytes.
constexpr size_t FRAME_BYTES = WIDTH * HEIGHT * 2;
size_t copyFrame(uint8_t *dst, size_t maxLen, size_t index);
uint32_t frameVersion();
void onChange(std::function<void()> cb);

// Best-effort UTF-8 -> ASCII (the bitmap fonts are ASCII only).
String ascii(const String &utf8);

}  // namespace display
