#include "web.h"

#include <AsyncJson.h>
#include <memory>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "actions.h"
#include "catalog.h"
#include "services/spotify.h"
#include "config.h"
#include "display.h"
#include "psram_json.h"
#include "presets.h"
#include "sonos/sonos.h"
#include "web_ui.h"

namespace web {

namespace {

AsyncWebServer server(80);
AsyncEventSource events("/events");

const char *phaseName(Phase p) {
  switch (p) {
    case Phase::Starting: return "starting";
    case Phase::Discovering: return "discovering";
    case Phase::NoSonos: return "no-sonos";
    case Phase::Ready: return "ready";
  }
  return "?";
}

String frameJson() { return String("{\"v\":") + display::frameVersion() + "}"; }

void handleScreen(AsyncWebServerRequest *req) {
  // Raw little-endian RGB565, display::WIDTH x display::HEIGHT, row-major.
  // Snapshot the whole frame first: the response goes out in chunks, and a new
  // frame drawn mid-transfer would otherwise show up torn.
  std::shared_ptr<uint8_t> snap((uint8_t *)ps_malloc(display::FRAME_BYTES), free);
  if (!snap) {
    req->send(503, "text/plain", "busy");
    return;
  }
  display::copyFrame(snap.get(), display::FRAME_BYTES, 0);
  AsyncWebServerResponse *res = req->beginResponse(
      "application/octet-stream", display::FRAME_BYTES,
      [snap](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
        if (index >= display::FRAME_BYTES) return 0;
        size_t n = min(maxLen, display::FRAME_BYTES - index);
        memcpy(buf, snap.get() + index, n);
        return n;
      });
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void sendJson(AsyncWebServerRequest *req, JsonDocument &doc) {
  String out;
  out.reserve(measureJson(doc) + 1);  // one allocation; large ones land in PSRAM
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

void handleState(AsyncWebServerRequest *req) {
  JsonDocument doc;
  doc["phase"] = phaseName(actions::phase());
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  auto rooms = sonos::rooms();
  doc["rooms"] = rooms.size();
  doc["favourites"] = sonos::favourites().size();
  JsonArray grp = doc["group"].to<JsonArray>();
  for (auto &u : actions::currentGroup()) {
    Room r;
    grp.add(sonos::findRoom(u, r) ? r.name : u);
  }
  doc["frame"] = display::frameVersion();
  sendJson(req, doc);
}

void handleRooms(AsyncWebServerRequest *req) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto &r : sonos::rooms()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = r.name;
    o["uuid"] = r.uuid;
    o["coordinator"] = r.coordinator;
  }
  sendJson(req, doc);
}

// Append s as a JSON string literal.
void jsonStr(String &out, const String &s) {
  out += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  out += '"';
}

void handleFavourites(AsyncWebServerRequest *req) {
  // Hundreds of items: write the JSON text straight into one big buffer
  // (>4 KB, so it lands in PSRAM) instead of building a JsonDocument.
  String out;
  out.reserve(96 * 1024);
  out += '[';
  bool first = true;
  catalog::forEach([&](const Favourite &f) {
    if (!first) out += ',';
    first = false;
    out += "{\"title\":";
    jsonStr(out, f.title);
    out += ",\"uri\":";
    jsonStr(out, f.uri);
    out += ",\"group\":";
    jsonStr(out, f.group);
    if (f.art.length()) {
      out += ",\"art\":";
      jsonStr(out, f.art);
    }
    String why = unsupportedReason(f);
    if (why.length()) {
      out += ",\"unsupported\":";
      jsonStr(out, why);
    }
    out += '}';
  });
  out += ']';
  req->send(200, "application/json", out);
}

void handlePresets(AsyncWebServerRequest *req) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  presets::toJson(arr);
  for (JsonObject p : arr) {  // cover for the preset tile
    Favourite f;
    String uri = p["favUri"] | "";
    if (uri.length() && catalog::find(uri, f) && f.art.length()) p["art"] = f.art;
  }
  sendJson(req, doc);
}

String nowJson() {
  JsonDocument doc;
  actions::nowPlayingJson(doc.to<JsonObject>());
  String out;
  serializeJson(doc, out);
  return out;
}

void handleServices(AsyncWebServerRequest *req) {
  JsonDocument doc;
  spotify::statusJson(doc["spotify"].to<JsonObject>());
  sendJson(req, doc);
}

String formParam(AsyncWebServerRequest *req, const char *name) {
  return req->hasParam(name, true) ? req->getParam(name, true)->value() : String();
}

void replyOk(AsyncWebServerRequest *req, bool ok, const String &error = String(),
             const String &url = String()) {
  JsonDocument doc;
  doc["ok"] = ok;
  if (error.length()) doc["error"] = error;
  if (url.length()) doc["url"] = url;
  String out;
  serializeJson(doc, out);
  req->send(ok ? 200 : 400, "application/json", out);
}

void registerServiceRoutes() {
  server.on("/api/services", HTTP_GET, handleServices);
  server.on("/api/services/spotify/client", HTTP_POST, [](AsyncWebServerRequest *req) {
    String id = formParam(req, "clientId");
    id.trim();
    if (id.length() && id.length() != 32)  // empty clears it
      return replyOk(req, false, "A Spotify client ID is 32 characters.");
    spotify::setClientId(id);
    replyOk(req, true);
  });
  server.on("/api/services/spotify/connect", HTTP_POST, [](AsyncWebServerRequest *req) {
    String url = spotify::authorizeUrl();
    if (url.isEmpty()) return replyOk(req, false, "Save your Spotify client ID first.");
    replyOk(req, true, String(), url);
  });
  server.on("/api/services/spotify/callback", HTTP_POST, [](AsyncWebServerRequest *req) {
    String err;
    if (!spotify::acceptRedirect(formParam(req, "url"), err)) return replyOk(req, false, err);
    actions::post(ActionType::Sync);
    replyOk(req, true);
  });
  server.on("/api/services/spotify/disconnect", HTTP_POST, [](AsyncWebServerRequest *req) {
    spotify::disconnect();
    replyOk(req, true);
  });
}

void handleAction(AsyncWebServerRequest *req) {
  ActionType t;
  if (!req->hasParam("type") || !actions::parse(req->getParam("type")->value(), t)) {
    req->send(400, "text/plain", "bad type");
    return;
  }
  int n = req->hasParam("n") ? req->getParam("n")->value().toInt() : 0;
  if (t == ActionType::Preset && (n < 0 || n >= NUM_PRESETS)) {
    req->send(400, "text/plain", "bad preset");
    return;
  }
  if (t == ActionType::Volume) n = constrain(n, 0, 100);
  bool ok = actions::post(t, n);
  req->send(ok ? 200 : 503, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

}  // namespace

void begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", (const uint8_t *)INDEX_HTML, strlen(INDEX_HTML));
  });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/rooms", HTTP_GET, handleRooms);
  server.on("/api/favourites", HTTP_GET, handleFavourites);
  server.on("/api/presets", HTTP_GET, handlePresets);
  server.on("/api/nowplaying", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", nowJson());
  });
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/screen", HTTP_GET, handleScreen);
  registerServiceRoutes();

  auto *save = new AsyncCallbackJsonWebHandler("/api/presets", [](AsyncWebServerRequest *req,
                                                                  JsonVariant &json) {
    if (!json.is<JsonArray>()) {
      req->send(400, "text/plain", "expected array");
      return;
    }
    bool ok = presets::fromJson(json.as<JsonArrayConst>());
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });
  save->setMethod(HTTP_POST);
  save->setMaxContentLength(16384);
  server.addHandler(save);

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send(frameJson().c_str(), "frame", millis());
    client->send(nowJson().c_str(), "np", millis());
  });
  server.addHandler(&events);
  display::onChange([] { events.send(frameJson().c_str(), "frame", millis()); });
  actions::onNowPlayingChange([] { events.send(nowJson().c_str(), "np", millis()); });

  server.onNotFound([](AsyncWebServerRequest *req) { req->send(404, "text/plain", "not found"); });
  server.begin();
}

}  // namespace web
