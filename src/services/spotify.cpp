#include "spotify.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

#include "../psram_json.h"
#include "spotify_certs.h"

namespace spotify {

namespace {

constexpr const char *SCOPES = "playlist-read-private playlist-read-collaborative user-library-read";
constexpr int MAX_PLAYLISTS = 200;

SemaphoreHandle_t mtx;
struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

// Persistent
String g_clientId, g_refresh;
// Session
String g_access;
uint32_t g_accessExpiry = 0;
String g_verifier, g_state, g_pendingCode;
String g_user, g_error;
uint32_t g_updatedAt = 0;  // millis() of last successful refresh
bool g_busy = false;
std::vector<Item> g_items;

// ---- small utilities --------------------------------------------------------

void save() {
  Preferences p;
  p.begin("spotify", false);
  p.putString("client", g_clientId);
  p.putString("refresh", g_refresh);
  p.end();
}

String urlEncode(const String &s) {
  static const char *hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

String urlDecode(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '%' && i + 2 < s.length()) {
      out += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16);
      i += 2;
    } else out += s[i] == '+' ? ' ' : s[i];
  }
  return out;
}

String queryParam(const String &url, const char *name) {
  String key = String(name) + "=";
  int q = url.indexOf('?');
  int p = q;
  while (p >= 0) {
    int start = p + 1;
    if (url.startsWith(key, start)) {
      int end = url.indexOf('&', start);
      if (end < 0) end = url.indexOf('#', start);
      if (end < 0) end = url.length();
      return urlDecode(url.substring(start + key.length(), end));
    }
    p = url.indexOf('&', start);
  }
  return String();
}

String randomString(int len) {
  static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  String s;
  for (int i = 0; i < len; i++) s += chars[esp_random() % 66];
  return s;
}

String base64url(const uint8_t *data, size_t len) {
  static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  String out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = data[i] << 16 | (i + 1 < len ? data[i + 1] << 8 : 0) | (i + 2 < len ? data[i + 2] : 0);
    out += tbl[(n >> 18) & 63];
    out += tbl[(n >> 12) & 63];
    if (i + 1 < len) out += tbl[(n >> 6) & 63];
    if (i + 2 < len) out += tbl[n & 63];
  }
  return out;
}

String pkceChallenge(const String &verifier) {
  uint8_t hash[32];
#if MBEDTLS_VERSION_MAJOR >= 3
  mbedtls_sha256((const uint8_t *)verifier.c_str(), verifier.length(), hash, 0);
#else
  mbedtls_sha256_ret((const uint8_t *)verifier.c_str(), verifier.length(), hash, 0);
#endif
  return base64url(hash, sizeof(hash));
}

// ---- HTTPS ------------------------------------------------------------------

// POST to the token endpoint. Returns HTTP status (or <0 on transport error).
int tokenRequest(const String &form, JsonDocument &doc) {
  WiFiClientSecure tls;
  tls.setCACert(SPOTIFY_ROOT_CAS);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (!http.begin(tls, "https://accounts.spotify.com/api/token")) return -1;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(form);
  if (code > 0) deserializeJson(doc, http.getStream());
  http.end();
  return code;
}

// GET a Web API URL with a response filter. Returns HTTP status.
int apiGet(const String &url, JsonDocument &doc, JsonDocument &filter) {
  WiFiClientSecure tls;
  tls.setCACert(SPOTIFY_ROOT_CAS);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so we can stream-parse
  http.setTimeout(10000);
  if (!http.begin(tls, url)) return -1;
  http.addHeader("Authorization", "Bearer " + g_access);
  int code = http.GET();
  if (code == 200) {
    DeserializationError err =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    if (err) {
      Serial.printf("[spotify] JSON error %s for %s\n", err.c_str(), url.c_str());
      code = -2;
    }
  } else {
    Serial.printf("[spotify] GET %s -> %d\n", url.c_str(), code);
  }
  http.end();
  return code;
}

bool ensureToken() {
  if (g_access.length() && (int32_t)(g_accessExpiry - millis()) > 60000) return true;
  String refresh, client;
  {
    Lock l;
    refresh = g_refresh;
    client = g_clientId;
  }
  if (refresh.isEmpty() || client.isEmpty()) return false;

  JsonDocument doc;
  int code = tokenRequest("grant_type=refresh_token&refresh_token=" + urlEncode(refresh) +
                              "&client_id=" + urlEncode(client),
                          doc);
  if (code != 200) {
    String err = doc["error"] | "";
    Serial.printf("[spotify] token refresh failed: %d %s\n", code, err.c_str());
    Lock l;
    if (err == "invalid_grant" || err == "invalid_client") {
      g_refresh = "";
      g_error = "Spotify sign-in expired. Connect again.";
      save();
    } else {
      g_error = "Couldn't reach Spotify (" + String(code) + ")";
    }
    return false;
  }
  Lock l;
  g_access = doc["access_token"] | "";
  g_accessExpiry = millis() + (uint32_t)(doc["expires_in"] | 3600) * 1000;
  String rotated = doc["refresh_token"] | "";
  if (rotated.length() && rotated != g_refresh) {
    g_refresh = rotated;  // PKCE refresh tokens rotate
    save();
  }
  return g_access.length() > 0;
}

void addAlbums(JsonArrayConst arr, const char *albumKey, const char *group, std::vector<Item> &out) {
  for (JsonVariantConst v : arr) {
    JsonVariantConst a = albumKey ? v[albumKey] : v;
    const char *id = a["id"];
    if (!id) continue;
    String title = a["name"] | "";
    const char *artist = a["artists"][0]["name"];
    if (artist) title += String(" - ") + artist;
    out.push_back({"album", id, title, group});
  }
}

}  // namespace

// ---- public -------------------------------------------------------------------

void begin() {
  mtx = xSemaphoreCreateMutex();
  Preferences p;
  p.begin("spotify", true);
  g_clientId = p.getString("client", "");
  g_refresh = p.getString("refresh", "");
  p.end();
}

void setClientId(const String &id) {
  Lock l;
  String trimmed = id;
  trimmed.trim();
  if (trimmed == g_clientId) return;
  g_clientId = trimmed;
  g_refresh = "";  // tokens belong to the old app
  g_access = "";
  g_items.clear();
  g_error = "";
  save();
}

String authorizeUrl() {
  Lock l;
  if (g_clientId.isEmpty()) return String();
  g_verifier = randomString(64);
  g_state = randomString(16);
  return "https://accounts.spotify.com/authorize?response_type=code&client_id=" +
         urlEncode(g_clientId) + "&scope=" + urlEncode(SCOPES) +
         "&redirect_uri=" + urlEncode(REDIRECT_URI) +
         "&code_challenge_method=S256&code_challenge=" + pkceChallenge(g_verifier) +
         "&state=" + g_state;
}

bool acceptRedirect(const String &url, String &err) {
  Lock l;
  String denied = queryParam(url, "error");
  if (denied.length()) {
    err = "Spotify said: " + denied;
    return false;
  }
  String code = queryParam(url, "code"), state = queryParam(url, "state");
  if (code.isEmpty()) {
    err = "That address has no code in it. Copy the whole address from the browser bar.";
    return false;
  }
  if (g_verifier.isEmpty() || state != g_state) {
    err = "That sign-in link is out of date. Press Connect and try again.";
    return false;
  }
  g_pendingCode = code;
  g_error = "";
  return true;
}

void finishSignIn() {
  String code, verifier, client;
  {
    Lock l;
    if (g_pendingCode.isEmpty()) return;
    code = g_pendingCode;
    verifier = g_verifier;
    client = g_clientId;
    g_pendingCode = "";
    g_busy = true;
  }
  JsonDocument doc;
  int status = tokenRequest("grant_type=authorization_code&code=" + urlEncode(code) +
                                "&redirect_uri=" + urlEncode(REDIRECT_URI) +
                                "&client_id=" + urlEncode(client) +
                                "&code_verifier=" + urlEncode(verifier),
                            doc);
  Lock l;
  g_busy = false;
  if (status != 200) {
    String e = doc["error_description"] | doc["error"] | "";
    g_error = "Sign-in failed (" + String(status) + (e.length() ? ": " + e : String()) + ")";
    Serial.printf("[spotify] %s\n", g_error.c_str());
    return;
  }
  g_access = doc["access_token"] | "";
  g_accessExpiry = millis() + (uint32_t)(doc["expires_in"] | 3600) * 1000;
  g_refresh = doc["refresh_token"] | "";
  g_verifier = "";
  g_state = "";
  save();
  Serial.println("[spotify] signed in");
}

bool connected() {
  Lock l;
  return g_refresh.length() > 0;
}

bool refreshCatalog() {
  if (!connected() || !ensureToken()) return false;
  {
    Lock l;
    g_busy = true;
  }
  std::vector<Item> list;
  String user;
  bool ok = true;

  {  // Who are we?
    JsonDocument filter, doc(PsramAllocator::instance());
    filter["display_name"] = true;
    filter["id"] = true;
    if (apiGet("https://api.spotify.com/v1/me", doc, filter) == 200)
      user = doc["display_name"] | doc["id"] | "";
    else ok = false;
  }

  // Your playlists (own + followed, including Spotify's that you follow).
  for (int offset = 0; ok && offset < MAX_PLAYLISTS; offset += 50) {
    JsonDocument filter, doc(PsramAllocator::instance());
    filter["items"][0]["id"] = true;
    filter["items"][0]["name"] = true;
    filter["next"] = true;
    if (apiGet("https://api.spotify.com/v1/me/playlists?limit=50&offset=" + String(offset), doc,
               filter) != 200) {
      ok = false;
      break;
    }
    for (JsonVariantConst p : doc["items"].as<JsonArrayConst>()) {
      const char *id = p["id"];
      if (id) list.push_back({"playlist", id, p["name"] | "", "Spotify - Your playlists"});
    }
    if (doc["next"].isNull()) break;
  }

  if (ok) {  // Saved albums
    JsonDocument filter, doc(PsramAllocator::instance());
    filter["items"][0]["album"]["id"] = true;
    filter["items"][0]["album"]["name"] = true;
    filter["items"][0]["album"]["artists"][0]["name"] = true;
    if (apiGet("https://api.spotify.com/v1/me/albums?limit=50", doc, filter) == 200)
      addAlbums(doc["items"], "album", "Spotify - Saved albums", list);
  }

  if (ok) {  // Spotify's new releases (optional: may not be available to every app)
    JsonDocument filter, doc(PsramAllocator::instance());
    filter["albums"]["items"][0]["id"] = true;
    filter["albums"]["items"][0]["name"] = true;
    filter["albums"]["items"][0]["artists"][0]["name"] = true;
    if (apiGet("https://api.spotify.com/v1/browse/new-releases?limit=30", doc, filter) == 200)
      addAlbums(doc["albums"]["items"], nullptr, "Spotify - New releases", list);
  }

  Lock l;
  g_busy = false;
  if (!ok) {
    if (g_error.isEmpty()) g_error = "Couldn't load Spotify lists";
    return false;
  }
  g_items = std::move(list);
  g_user = user;
  g_error = "";
  g_updatedAt = millis();
  Serial.printf("[spotify] %u items for %s\n", (unsigned)g_items.size(), g_user.c_str());
  return true;
}

std::vector<Item> items() {
  Lock l;
  return g_items;
}

void disconnect() {
  Lock l;
  g_refresh = "";
  g_access = "";
  g_items.clear();
  g_user = "";
  g_error = "";
  save();
}

void statusJson(JsonObject o) {
  Lock l;
  o["clientId"] = g_clientId;
  o["connected"] = g_refresh.length() > 0;
  o["busy"] = g_busy || g_pendingCode.length() > 0;
  o["user"] = g_user;
  o["error"] = g_error;
  o["items"] = g_items.size();
  o["updatedSecondsAgo"] = g_updatedAt ? (int)((millis() - g_updatedAt) / 1000) : -1;
  o["redirectUri"] = REDIRECT_URI;
}

}  // namespace spotify
