#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>

#include <vector>

// Everything the controller can do. Buttons and the web page post actions to a
// queue; a single worker task runs them (and polls now-playing) so slow network
// calls never block button reads or the web server.
enum class ActionType : uint8_t {
  Preset, PlayPause, Stop, VolUp, VolDown, Next, Random, Rescan,
  Sync,    // finish a music-service sign-in and reload its lists
  Volume,  // absolute group volume; index = 0..100
};

enum class Phase : uint8_t { Starting, Discovering, NoSonos, Ready };

namespace actions {

void begin();
bool post(ActionType type, uint8_t index = 0);

const char *name(ActionType t);
bool parse(const String &s, ActionType &out);

Phase phase();
// Room UUIDs the transport buttons act on (set by the last preset / random).
std::vector<String> currentGroup();
// millis() of the last action handled (for the LED flicker).
uint32_t lastActivity();

// What's playing on the current group, for the web UI (title, artist, album,
// art, state, volume, group, preset). Updated by the worker on every poll.
void nowPlayingJson(JsonObject out);
// Called (from the worker task) whenever nowPlayingJson would change.
void onNowPlayingChange(std::function<void()> cb);

}  // namespace actions
