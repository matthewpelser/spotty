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

void handleFavourites(AsyncWebServerRequest *req) {
  JsonDocument doc(PsramAllocator::instance());
  JsonArray arr = doc.to<JsonArray>();
  catalog::forEach([&](const Favourite &f) {
    JsonObject o = arr.add<JsonObject>();
    o["title"] = f.title;
    o["uri"] = f.uri;
    o["group"] = f.group;
    String why = unsupportedReason(f);
    if (why.length()) o["unsupported"] = why;
  });
  sendJson(req, doc);
}

void handlePresets(AsyncWebServerRequest *req) {
  JsonDocument doc;
  presets::toJson(doc.to<JsonArray>());
  sendJson(req, doc);
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
  });
  server.addHandler(&events);
  display::onChange([] { events.send(frameJson().c_str(), "frame", millis()); });

  server.onNotFound([](AsyncWebServerRequest *req) { req->send(404, "text/plain", "not found"); });
  server.begin();
}

}  // namespace web
