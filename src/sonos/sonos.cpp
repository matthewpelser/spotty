#include "sonos.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <algorithm>

#include "soap.h"

String unsupportedReason(const Favourite &fav) {
  if (fav.uri.startsWith("spotify:")) return "Sonos has no Spotify link";
  if (fav.uri.startsWith("x-sonosapi-stream:") &&
      (fav.uri.indexOf("sid=333") >= 0 || fav.uri.indexOf("sid=254") >= 0))
    return "TuneIn";
  return String();
}

namespace sonos {

namespace {

String g_playError;

SemaphoreHandle_t mtx;
std::vector<Room> g_rooms;
std::vector<Favourite> g_favs;
String g_lastIp;  // a player known to answer topology queries

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

const char *INSTANCE = "<InstanceID>0</InstanceID>";

void rememberIp(const String &ip) {
  if (ip == g_lastIp) return;
  g_lastIp = ip;
  Preferences p;
  p.begin("sonos", false);
  p.putString("ip", ip);
  p.end();
}

// SSDP M-SEARCH for Sonos ZonePlayers. Returns responder IPs.
std::vector<String> ssdpSearch() {
  std::vector<String> found;
  WiFiUDP udp;
  if (!udp.begin(51900)) return found;
  static const char msg[] =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "MX: 1\r\n"
      "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n\r\n";
  for (int attempt = 0; attempt < 2; attempt++) {
    udp.beginPacket(IPAddress(239, 255, 255, 250), 1900);
    udp.write((const uint8_t *)msg, sizeof(msg) - 1);
    udp.endPacket();
    uint32_t deadline = millis() + 1500;
    char buf[512];
    while ((int32_t)(deadline - millis()) > 0) {
      int n = udp.parsePacket();
      if (n <= 0) {
        delay(10);
        continue;
      }
      int len = udp.read(buf, sizeof(buf) - 1);
      buf[len > 0 ? len : 0] = 0;
      if (!strstr(buf, "ZonePlayer") && !strstr(buf, "Sonos")) continue;
      String ip = udp.remoteIP().toString();
      if (std::find(found.begin(), found.end(), ip) == found.end()) found.push_back(ip);
    }
    if (!found.empty()) break;
  }
  udp.stop();
  Serial.printf("[sonos] SSDP found %u player(s)\n", (unsigned)found.size());
  return found;
}

bool fetchTopology(const String &ip) {
  String resp;
  if (!soap::call(ip, soap::ZoneGroupTopology, "GetZoneGroupState", "", &resp)) return false;
  String state = soap::unescape(soap::tag(resp, "ZoneGroupState"));
  resp = String();

  std::vector<Room> list;
  int pos = 0;
  while (true) {
    int g = state.indexOf("<ZoneGroup ", pos);
    if (g < 0) break;
    int gEnd = state.indexOf("</ZoneGroup>", g);
    if (gEnd < 0) gEnd = state.length();
    String coord = soap::attr(state.substring(g, state.indexOf('>', g)), "Coordinator");

    int m = g;
    while (true) {
      m = state.indexOf("<ZoneGroupMember ", m);
      if (m < 0 || m > gEnd) break;
      int mgt = state.indexOf('>', m);
      String el = state.substring(m, mgt);
      m = mgt;
      // Skip hidden halves of stereo pairs / surrounds, and Bridges/Boosts.
      if (soap::attr(el, "Invisible") == "1" || soap::attr(el, "IsZoneBridge") == "1") continue;
      Room r;
      r.name = soap::unescape(soap::attr(el, "ZoneName"));
      r.uuid = soap::attr(el, "UUID");
      String loc = soap::attr(el, "Location");  // http://192.168.1.20:1400/xml/...
      int s = loc.indexOf("//");
      int e = loc.indexOf(':', s + 2);
      if (s >= 0 && e > s) r.ip = loc.substring(s + 2, e);
      r.coordinator = coord;
      if (r.name.length() && r.uuid.length() && r.ip.length()) list.push_back(r);
    }
    pos = gEnd;
  }
  if (list.empty()) return false;

  std::sort(list.begin(), list.end(),
            [](const Room &a, const Room &b) { return a.name < b.name; });
  {
    Lock l;
    g_rooms = std::move(list);
  }
  rememberIp(ip);
  return true;
}

bool isStream(const Favourite &f) {
  static const char *prefixes[] = {"x-sonosapi-stream:", "x-sonosapi-radio:", "x-rincon-mp3radio:",
                                   "x-sonosapi-hls:",    "x-sonosapi-hls-static:",
                                   "x-rincon-stream:",   "aac:",
                                   "hls-radio:"};
  for (auto p : prefixes)
    if (f.uri.startsWith(p)) return true;
  return f.meta.indexOf("audioBroadcast") >= 0;
}

bool avt(const Room &r, const char *action, const String &extra = String(),
         String *resp = nullptr) {
  return soap::call(r.ip, soap::AVTransport, action, INSTANCE + extra, resp);
}

}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  Preferences p;
  p.begin("sonos", true);
  g_lastIp = p.getString("ip", "");
  p.end();
}

bool discover() {
  if (g_lastIp.length() && fetchTopology(g_lastIp)) return true;
  for (auto &ip : ssdpSearch())
    if (fetchTopology(ip)) return true;
  return false;
}

bool refreshTopology() {
  if (g_lastIp.length() && fetchTopology(g_lastIp)) return true;
  for (auto &r : rooms())
    if (fetchTopology(r.ip)) return true;
  return discover();
}

bool loadFavourites() {
  if (g_lastIp.isEmpty()) return false;
  String resp;
  bool ok = soap::call(g_lastIp, soap::ContentDirectory, "Browse",
                       "<ObjectID>FV:2</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag>"
                       "<Filter>*</Filter><StartingIndex>0</StartingIndex>"
                       "<RequestedCount>200</RequestedCount><SortCriteria></SortCriteria>",
                       &resp);
  if (!ok) return false;
  String didl = soap::unescape(soap::tag(resp, "Result"));
  resp = String();

  std::vector<Favourite> list;
  int pos = 0;
  while (true) {
    int s = didl.indexOf("<item", pos);
    if (s < 0) break;
    int e = didl.indexOf("</item>", s);
    if (e < 0) break;
    String item = didl.substring(s, e);
    pos = e + 7;
    Favourite f;
    f.title = soap::unescape(soap::tag(item, "dc:title"));
    f.uri = soap::unescape(soap::tag(item, "res"));
    f.meta = soap::unescape(soap::tag(item, "r:resMD"));
    f.art = soap::unescape(soap::tag(item, "upnp:albumArtURI"));
    if (f.art.startsWith("/")) f.art = "http://" + g_lastIp + ":1400" + f.art;
    f.group = "Sonos Favourites";
    if (f.title.length() && f.uri.length()) list.push_back(f);
  }
  Serial.printf("[sonos] %u favourites\n", (unsigned)list.size());
  Lock l;
  g_favs = std::move(list);
  return true;
}

std::vector<Room> rooms() {
  Lock l;
  return g_rooms;
}

std::vector<Favourite> favourites() {
  Lock l;
  return g_favs;
}

bool findRoom(const String &uuid, Room &out) {
  Lock l;
  for (auto &r : g_rooms)
    if (r.uuid == uuid) {
      out = r;
      return true;
    }
  return false;
}

bool findFavourite(const String &uri, Favourite &out) {
  Lock l;
  for (auto &f : g_favs)
    if (f.uri == uri) {
      out = f;
      return true;
    }
  return false;
}

bool spotifyAccount(String &sn, String &desc) {
  Lock l;
  for (auto &f : g_favs) {
    if (f.uri.indexOf("sid=9&") < 0 || f.uri.indexOf("spotify") < 0) continue;
    int p = f.uri.indexOf("sn=");
    int d = f.meta.indexOf("<desc ");
    if (p < 0 || d < 0) continue;
    int pe = f.uri.indexOf('&', p);
    sn = f.uri.substring(p + 3, pe < 0 ? f.uri.length() : pe);
    int ds = f.meta.indexOf('>', d) + 1, de = f.meta.indexOf("</desc>", ds);
    if (ds <= 0 || de < 0) continue;
    desc = f.meta.substring(ds, de);
    return true;
  }
  return false;
}

String coordinatorOf(const String &uuid) {
  Room r;
  return findRoom(uuid, r) ? r.coordinator : uuid;
}

bool group(const std::vector<String> &uuids) {
  if (uuids.empty()) return false;
  refreshTopology();
  const String &coord = uuids[0];
  Room c;
  if (!findRoom(coord, c)) return false;
  auto inTarget = [&](const String &u) {
    return std::find(uuids.begin(), uuids.end(), u) != uuids.end();
  };

  // 1. Coordinator leaves any group it is currently a member of.
  bool coordLeads = c.coordinator == coord;
  if (!coordLeads) avt(c, "BecomeCoordinatorOfStandaloneGroup");

  // 2. Drop rooms following the coordinator that aren't in the preset.
  for (auto &r : rooms()) {
    if (coordLeads && r.uuid != coord && r.coordinator == coord && !inTarget(r.uuid))
      avt(r, "BecomeCoordinatorOfStandaloneGroup");
  }

  // 3. Join the rest.
  bool ok = true;
  for (size_t i = 1; i < uuids.size(); i++) {
    Room r;
    if (!findRoom(uuids[i], r)) continue;
    if (coordLeads && r.coordinator == coord) continue;  // already in
    ok &= avt(r, "SetAVTransportURI",
              "<CurrentURI>x-rincon:" + coord + "</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>");
  }
  refreshTopology();
  return ok;
}

void setVolumes(const std::vector<String> &uuids, int volume) {
  volume = constrain(volume, 0, 100);
  for (auto &u : uuids) {
    Room r;
    if (!findRoom(u, r)) continue;
    soap::call(r.ip, soap::RenderingControl, "SetVolume",
               String(INSTANCE) + "<Channel>Master</Channel><DesiredVolume>" + volume +
                   "</DesiredVolume>");
  }
}

String lastPlayError() { return g_playError; }

static bool playFavouriteImpl(const String &coordUuid, const Favourite &fav, bool shuffle);

bool playFavourite(const String &coordUuid, const Favourite &fav, bool shuffle) {
  g_playError = "";
  String why = unsupportedReason(fav);
  if (why.length()) {
    g_playError = why + " blocked";
    Serial.printf("[sonos] '%s' not playable locally (%s)\n", fav.title.c_str(), why.c_str());
    return false;
  }
  bool ok = playFavouriteImpl(coordUuid, fav, shuffle);
  if (!ok && g_playError.isEmpty()) g_playError = "Sonos error " + String(soap::lastError());
  return ok;
}

static bool playFavouriteImpl(const String &coordUuid, const Favourite &fav, bool shuffle) {
  Room c;
  if (!findRoom(coordUuid, c)) {
    g_playError = "Room offline";
    return false;
  }
  String uri = soap::escape(fav.uri);
  String meta = soap::escape(fav.meta);

  if (isStream(fav)) {
    if (!avt(c, "SetAVTransportURI",
             "<CurrentURI>" + uri + "</CurrentURI><CurrentURIMetaData>" + meta +
                 "</CurrentURIMetaData>"))
      return false;
  } else {
    avt(c, "RemoveAllTracksFromQueue");
    if (!avt(c, "AddURIToQueue",
             "<EnqueuedURI>" + uri + "</EnqueuedURI><EnqueuedURIMetaData>" + meta +
                 "</EnqueuedURIMetaData><DesiredFirstTrackNumberEnqueued>0"
                 "</DesiredFirstTrackNumberEnqueued><EnqueueAsNext>0</EnqueueAsNext>"))
      return false;
    if (!avt(c, "SetAVTransportURI",
             "<CurrentURI>x-rincon-queue:" + coordUuid +
                 "#0</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>"))
      return false;
    avt(c, "SetPlayMode",
        String("<NewPlayMode>") + (shuffle ? "SHUFFLE_NOREPEAT" : "NORMAL") + "</NewPlayMode>");
  }
  return avt(c, "Play", "<Speed>1</Speed>");
}

bool playPause(const String &coordUuid) {
  Room c;
  if (!findRoom(coordUuid, c)) return false;
  String resp;
  avt(c, "GetTransportInfo", "", &resp);
  String state = soap::tag(resp, "CurrentTransportState");
  if (state == "PLAYING" || state == "TRANSITIONING") {
    // Some radio streams can't pause; fall back to stop.
    return avt(c, "Pause") || avt(c, "Stop");
  }
  return avt(c, "Play", "<Speed>1</Speed>");
}

bool stop(const String &coordUuid) {
  Room c;
  return findRoom(coordUuid, c) && avt(c, "Stop");
}

bool next(const String &coordUuid) {
  Room c;
  return findRoom(coordUuid, c) && avt(c, "Next");
}

bool setGroupVolume(const String &coordUuid, int volume) {
  Room c;
  if (!findRoom(coordUuid, c)) return false;
  soap::call(c.ip, soap::GroupRenderingControl, "SnapshotGroupVolume", INSTANCE);
  return soap::call(c.ip, soap::GroupRenderingControl, "SetGroupVolume",
                    String(INSTANCE) + "<DesiredVolume>" + constrain(volume, 0, 100) +
                        "</DesiredVolume>");
}

int adjustGroupVolume(const String &coordUuid, int delta) {
  Room c;
  if (!findRoom(coordUuid, c)) return -1;
  String resp;
  if (!soap::call(c.ip, soap::GroupRenderingControl, "SetRelativeGroupVolume",
                  String(INSTANCE) + "<Adjustment>" + delta + "</Adjustment>", &resp))
    return -1;
  return soap::tag(resp, "NewVolume").toInt();
}

// Sonos gives a path on the player ("/getaa?...") or sometimes a full URL.
static String absoluteArt(const String &art, const String &ip) {
  if (art.startsWith("/")) return "http://" + ip + ":1400" + art;
  if (art.startsWith("http://")) return art;
  return String();  // https art isn't fetched
}

String nextArtUrl(const String &coordUuid) {
  Room c;
  if (!findRoom(coordUuid, c)) return String();
  String resp;
  if (!avt(c, "GetMediaInfo", "", &resp) ||
      !soap::tag(resp, "CurrentURI").startsWith("x-rincon-queue:"))
    return String();
  if (!avt(c, "GetTransportSettings", "", &resp) || soap::tag(resp, "PlayMode").indexOf("SHUFFLE") >= 0)
    return String();  // next track is unpredictable
  if (!avt(c, "GetPositionInfo", "", &resp)) return String();
  int track = soap::tag(resp, "Track").toInt();  // 1-based; next item's 0-based index
  if (track <= 0) return String();
  if (!soap::call(c.ip, soap::ContentDirectory, "Browse",
                  "<ObjectID>Q:0</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag>"
                  "<Filter>upnp:albumArtURI</Filter><StartingIndex>" + String(track) +
                      "</StartingIndex><RequestedCount>1</RequestedCount><SortCriteria></SortCriteria>",
                  &resp))
    return String();
  String didl = soap::unescape(soap::tag(resp, "Result"));
  return absoluteArt(soap::unescape(soap::tag(didl, "upnp:albumArtURI")), c.ip);
}

NowPlaying nowPlaying(const String &coordUuid) {
  NowPlaying np;
  Room c;
  if (!findRoom(coordUuid, c)) return np;
  String resp;
  if (!avt(c, "GetTransportInfo", "", &resp)) return np;
  np.ok = true;
  np.state = soap::tag(resp, "CurrentTransportState");

  if (avt(c, "GetPositionInfo", "", &resp)) {
    String meta = soap::unescape(soap::tag(resp, "TrackMetaData"));
    np.title = soap::unescape(soap::tag(meta, "dc:title"));
    np.artist = soap::unescape(soap::tag(meta, "dc:creator"));
    np.artUrl = soap::unescape(soap::tag(meta, "upnp:albumArtURI"));
    np.album = soap::unescape(soap::tag(meta, "upnp:album"));
    String stream = soap::unescape(soap::tag(meta, "r:streamContent"));
    if (stream.length() || np.title.isEmpty() || np.title.startsWith("x-")) {
      // Radio: show "now playing" text with the station name.
      if (avt(c, "GetMediaInfo", "", &resp)) {
        String mm = soap::unescape(soap::tag(resp, "CurrentURIMetaData"));
        String station = soap::unescape(soap::tag(mm, "dc:title"));
        if (np.artUrl.isEmpty()) np.artUrl = soap::unescape(soap::tag(mm, "upnp:albumArtURI"));
        if (stream.length()) {
          np.title = stream;
          np.artist = station;
        } else if (station.length()) {
          np.title = station;
          np.artist = "";
        }
      }
    }
  }

  np.artUrl = absoluteArt(np.artUrl, c.ip);

  if (soap::call(c.ip, soap::GroupRenderingControl, "GetGroupVolume", INSTANCE, &resp))
    np.volume = soap::tag(resp, "CurrentVolume").toInt();
  return np;
}

}  // namespace sonos
