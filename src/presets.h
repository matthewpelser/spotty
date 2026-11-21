#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

#include "sonos/sonos.h"

// A preset is a "scene": these rooms, at this volume, playing this favourite.
struct Preset {
  String name;
  std::vector<String> rooms;  // room UUIDs; rooms[0] is the group coordinator
  int volume = -1;            // 0-100, or -1 to leave volume alone
  Favourite favourite;
  bool shuffle = false;

  bool configured() const { return !rooms.empty() && favourite.uri.length(); }
};

namespace presets {

void begin();  // load /presets.json from LittleFS
Preset get(int index);

// JSON shape used by the web UI (favourite metadata is not exposed).
void toJson(JsonArray out);
// Replace all presets from the web UI's JSON; favourite metadata is looked up
// from the Sonos favourites list by URI. Saves to flash.
bool fromJson(JsonArrayConst in);

}  // namespace presets
