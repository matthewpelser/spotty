#include "presets.h"

#include <LittleFS.h>

#include "catalog.h"
#include "config.h"

namespace presets {

namespace {

constexpr const char *PATH = "/presets.json";
SemaphoreHandle_t mtx;
Preset g_presets[NUM_PRESETS];

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

void defaults() {
  for (int i = 0; i < NUM_PRESETS; i++) {
    g_presets[i] = Preset();
    g_presets[i].name = "Preset " + String(i + 1);
  }
}

void readPreset(JsonObjectConst o, Preset &p) {
  p.name = o["name"] | "";
  p.rooms.clear();
  for (JsonVariantConst r : o["rooms"].as<JsonArrayConst>()) p.rooms.push_back(r.as<String>());
  p.volume = o["volume"] | -1;
  p.shuffle = o["shuffle"] | false;
  p.favourite.uri = o["favUri"] | "";
  p.favourite.title = o["favTitle"] | "";
  p.favourite.meta = o["favMeta"] | "";
}

void writePreset(JsonObject o, const Preset &p, bool withMeta) {
  o["name"] = p.name;
  JsonArray rooms = o["rooms"].to<JsonArray>();
  for (auto &r : p.rooms) rooms.add(r);
  o["volume"] = p.volume;
  o["shuffle"] = p.shuffle;
  o["favUri"] = p.favourite.uri;
  o["favTitle"] = p.favourite.title;
  if (withMeta) o["favMeta"] = p.favourite.meta;
  else if (unsupportedReason(p.favourite).length()) o["favUnsupported"] = unsupportedReason(p.favourite);
}

bool save() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  {
    Lock l;
    for (auto &p : g_presets) writePreset(arr.add<JsonObject>(), p, true);
  }
  File f = LittleFS.open(PATH, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  defaults();
  File f = LittleFS.open(PATH, "r");
  if (!f) return;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[presets] parse error: %s\n", err.c_str());
    return;
  }
  JsonArrayConst arr = doc.as<JsonArrayConst>();
  for (int i = 0; i < NUM_PRESETS && i < (int)arr.size(); i++) readPreset(arr[i], g_presets[i]);
  Serial.println("[presets] loaded");
}

Preset get(int index) {
  Lock l;
  return (index >= 0 && index < NUM_PRESETS) ? g_presets[index] : Preset();
}

void toJson(JsonArray out) {
  Lock l;
  for (auto &p : g_presets) writePreset(out.add<JsonObject>(), p, false);
}

bool fromJson(JsonArrayConst in) {
  Preset next[NUM_PRESETS];
  for (int i = 0; i < NUM_PRESETS; i++) {
    next[i] = get(i);
    if (i >= (int)in.size()) continue;
    Preset old = next[i];
    readPreset(in[i], next[i]);
    next[i].volume = constrain(next[i].volume, -1, 100);
    if (next[i].name.isEmpty()) next[i].name = "Preset " + String(i + 1);

    // Resolve favourite metadata from Sonos; keep the old copy if unchanged.
    Favourite fav;
    if (next[i].favourite.uri.isEmpty()) {
      next[i].favourite = Favourite();
    } else if (catalog::find(next[i].favourite.uri, fav)) {
      next[i].favourite = fav;
    } else if (next[i].favourite.uri == old.favourite.uri) {
      next[i].favourite = old.favourite;
    }
  }
  {
    Lock l;
    for (int i = 0; i < NUM_PRESETS; i++) g_presets[i] = next[i];
  }
  return save();
}

}  // namespace presets
