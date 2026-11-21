#pragma once
#include <Arduino.h>

#include <vector>

// High-level Sonos operations over the local network (UPnP, port 1400).
// All network calls are blocking: run them from the action worker task only.
// The accessors (rooms(), favourites(), ...) return copies and are safe from any task.

struct Room {
  String name;
  String uuid;         // RINCON_xxxxxxxxxxxx01400
  String ip;
  String coordinator;  // uuid of this room's current group coordinator
};

struct Favourite {
  String title;
  String uri;
  String meta;   // DIDL-Lite metadata (unescaped)
  String group;  // where it came from, for the UI ("Sonos Favourites", ...)
};

// Why a favourite can't be played over local UPnP control, or "" if it can.
// TuneIn stations (sid=333/254) are rejected with UPnP 402 by current Sonos
// firmware; they only play from the Sonos app / cloud API.
String unsupportedReason(const Favourite &fav);

struct NowPlaying {
  bool ok = false;
  String state;  // PLAYING, PAUSED_PLAYBACK, STOPPED, TRANSITIONING
  String title;
  String artist;
  String album;
  String artUrl;  // absolute http URL of the cover (served by the player), or ""
  int volume = -1;
};

namespace sonos {

void begin();

// SSDP + topology. Tries the last-known player first. Returns true if rooms were found.
bool discover();
// Re-read group topology from a known player (cheap). Returns false if none reachable.
bool refreshTopology();
bool loadFavourites();

std::vector<Room> rooms();
std::vector<Favourite> favourites();
bool findRoom(const String &uuid, Room &out);
bool findFavourite(const String &uri, Favourite &out);
// Sonos's link to Spotify, taken from any Spotify item in Sonos Favourites:
// the "sn" account number and the DIDL <desc> token. False if none found.
bool spotifyAccount(String &sn, String &desc);

// Make exactly `uuids` one group, coordinated by uuids[0].
bool group(const std::vector<String> &uuids);
// Set each room's own volume (0-100).
void setVolumes(const std::vector<String> &uuids, int volume);
// Play a favourite on the group led by `coordUuid`.
bool playFavourite(const String &coordUuid, const Favourite &fav, bool shuffle);
// Short (<=16 char) reason the last playFavourite() failed, for the LCD.
String lastPlayError();

// Uuid of the group coordinator that currently leads `uuid`'s group.
String coordinatorOf(const String &uuid);

bool playPause(const String &coordUuid);
bool stop(const String &coordUuid);
bool next(const String &coordUuid);
// Relative group volume change; returns the new volume or -1.
int adjustGroupVolume(const String &coordUuid, int delta);

NowPlaying nowPlaying(const String &coordUuid);
// Cover URL of the track after the current one in the group's queue, or "" if
// not playing from the queue, shuffled, or at the end.
String nextArtUrl(const String &coordUuid);

}  // namespace sonos
