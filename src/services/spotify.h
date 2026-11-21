#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

// Optional Spotify sign-in, used only to *list* things to play (your playlists,
// saved albums, Spotify's new releases). Playback still goes through Sonos,
// which uses its own Spotify link.
//
// Sign-in is OAuth "authorization code + PKCE" with a Spotify developer app
// the user creates. Spotify only allows https or loopback redirect URIs, so the
// browser is sent to http://127.0.0.1:8888/callback (which won't load) and the
// user pastes that address back into the Spotty page.
namespace spotify {

constexpr const char *REDIRECT_URI = "http://127.0.0.1:8888/callback";

struct Item {
  String kind;  // "playlist" | "album"
  String id;    // Spotify ID
  String title;
  const char *group;  // dropdown section (static string), e.g. "Spotify - Your playlists"
};

void begin();

// --- called from the web server (fast, no network) ---
void setClientId(const String &id);
String authorizeUrl();                       // starts a sign-in; "" if no client ID
bool acceptRedirect(const String &url, String &err);  // pasted callback URL
void disconnect();
void statusJson(JsonObject out);

// --- called from the action worker (network) ---
bool connected();
void finishSignIn();    // exchange a pending authorization code, if any
bool refreshCatalog();  // re-download the lists
std::vector<Item> items();

}  // namespace spotify
