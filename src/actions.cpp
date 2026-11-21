#include "actions.h"

#include <Preferences.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "art.h"
#include "presets.h"
#include "services/spotify.h"
#include "sonos/sonos.h"

namespace actions {

namespace {

constexpr uint32_t REDISCOVER_MS = 30000;
constexpr uint32_t TOPOLOGY_REFRESH_MS = 30000;
constexpr uint32_t MESSAGE_MS = 3000;
constexpr uint32_t SERVICES_REFRESH_MS = 30 * 60 * 1000;
uint32_t g_lastServices = 0;

struct Action {
  ActionType type;
  uint8_t index;
};

const char *NAMES[] = {"preset", "playpause", "stop", "volup", "voldown", "next", "random", "rescan", "sync", "volume"};

QueueHandle_t g_queue;
SemaphoreHandle_t g_mtx;
volatile Phase g_phase = Phase::Starting;
volatile uint32_t g_lastActivity = 0;
std::vector<String> g_group;
int g_lastRandom = -1;

// Transient screen (message / volume) shown instead of now-playing until then.
uint32_t g_msgUntil = 0;
uint32_t g_quickPollAt = 0;  // re-check soon after a transport command
constexpr int RANDOM_BADGE = 100;
int g_activePreset = -1;  // preset index, RANDOM_BADGE, or -1

void setActivePreset(int v) {
  if (v == g_activePreset) return;
  g_activePreset = v;
  Preferences p;
  p.begin("spotty", false);
  p.putInt("active", v);
  p.end();
}

// Now-playing snapshot shared with the web server.
struct NowState {
  bool ok = false;
  String state, title, artist, album, art, group, badge;
  int volume = -1;
  int preset = -1;
  bool operator==(const NowState &o) const {
    return ok == o.ok && state == o.state && title == o.title && artist == o.artist &&
           album == o.album && art == o.art && group == o.group && badge == o.badge &&
           volume == o.volume && preset == o.preset;
  }
};
NowState g_now;
std::function<void()> g_onNow;

void publishNow(const NowState &n) {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  bool changed = !(n == g_now);
  if (changed) g_now = n;
  xSemaphoreGive(g_mtx);
  if (changed && g_onNow) g_onNow();
}

void publishVolume(int vol) {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  NowState n = g_now;
  xSemaphoreGive(g_mtx);
  n.volume = vol;
  publishNow(n);
}

// ---- helpers ---------------------------------------------------------------

// Text for the screen's preset pill.
String presetBadge() {
  if (g_activePreset == RANDOM_BADGE) return "Random";
  if (g_activePreset < 0) return "";
  return presets::get(g_activePreset).name;
}

void notify(const String &title, const String &detail,
             display::Tone tone = display::Tone::Info, uint32_t ms = MESSAGE_MS) {
  g_msgUntil = millis() + ms;
  display::message(title, detail, tone);
}

void setGroup(const std::vector<String> &uuids) {
  String csv;
  for (auto &u : uuids) csv += (csv.length() ? "," : "") + u;
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  g_group = uuids;
  xSemaphoreGive(g_mtx);
  Preferences p;
  p.begin("spotty", false);
  p.putString("group", csv);
  p.end();
}

void loadGroup() {
  Preferences p;
  p.begin("spotty", true);
  String csv = p.getString("group", "");
  g_activePreset = p.getInt("active", -1);
  p.end();
  g_group.clear();
  int s = 0;
  while (s < (int)csv.length()) {
    int e = csv.indexOf(',', s);
    if (e < 0) e = csv.length();
    if (e > s) g_group.push_back(csv.substring(s, e));
    s = e + 1;
  }
}

// Coordinator the transport buttons should talk to, or "" if none.
String targetCoordinator() {
  std::vector<String> grp = currentGroup();
  if (grp.empty()) grp = presets::get(0).rooms;
  return grp.empty() ? String() : sonos::coordinatorOf(grp[0]);
}

// "Kitchen", "Kitchen + Bedroom" or "All rooms" for the group led by `coord`.
String groupLabel(const String &coord) {
  auto all = sonos::rooms();
  String lead, others;
  int n = 0;
  for (auto &r : all) {
    if (r.coordinator != coord) continue;
    n++;
    if (r.uuid == coord) lead = r.name;
    else others += " + " + r.name;
  }
  if (n == 0) return "No group";
  if (n == (int)all.size() && n > 1) return "All rooms";
  return lead + others;
}

void renderNowPlaying() {
  if (WiFi.status() != WL_CONNECTED) {
    notify("WiFi lost", "Reconnecting...", display::Tone::Error, 0);
    return;
  }
  if ((int32_t)(g_msgUntil - millis()) > 0) return;  // transient screen still showing

  String coord = targetCoordinator();
  if (coord.isEmpty()) {
    notify("Spotty ready", "Press a preset to start", display::Tone::Info, 0);
    return;
  }
  NowPlaying np = sonos::nowPlaying(coord);
  if (!np.ok) {
    notify("Sonos not responding", groupLabel(coord), display::Tone::Error, 0);
    return;
  }
  display::NowPlayingView v;
  v.group = groupLabel(coord);
  v.title = np.title;
  v.artist = np.artist;
  v.state = np.state;
  v.volume = np.volume;
  v.artUrl = np.artUrl;
  v.albumKey = np.album.length() ? np.artist + "|" + np.album : String();
  v.badge = presetBadge();

  NowState n;
  n.ok = true;
  n.state = np.state;
  n.title = np.title;
  n.artist = np.artist;
  n.album = np.album;
  n.art = np.artUrl;
  n.group = v.group;
  n.badge = v.badge;
  n.volume = np.volume;
  n.preset = g_activePreset == RANDOM_BADGE ? -1 : g_activePreset;
  publishNow(n);

  display::nowPlaying(v);

  // Once per track: load the next queue track's cover in the background so the
  // next track change is instant.
  static String prefetchedFor;
  if (np.state == "PLAYING" && np.artUrl.length() && np.artUrl != prefetchedFor) {
    prefetchedFor = np.artUrl;
    String next = sonos::nextArtUrl(coord);
    if (next.length() && next != np.artUrl) art::prefetch(next);
  }
}

// ---- action handlers -------------------------------------------------------

void syncServices(bool announce) {
  spotify::finishSignIn();
  if (!spotify::connected()) return;
  if (announce) notify("Spotify", "Loading your playlists", display::Tone::Busy);
  bool ok = spotify::refreshCatalog();
  g_lastServices = millis();
  if (announce)
    notify("Spotify", ok ? String(spotify::items().size()) + " playlists and albums" : String("Couldn't load lists"),
           ok ? display::Tone::Info : display::Tone::Error);
}


void runPreset(int i) {
  Preset p = presets::get(i);
  if (!p.configured()) {
    notify("Preset " + String(i + 1) + " is empty", "Set it up on the web page", display::Tone::Error);
    return;
  }
  if (unsupportedReason(p.favourite).length()) {
    // Don't regroup/change volume for something we know won't play.
    notify(p.name, unsupportedReason(p.favourite) + " favourites can't be started by the controller", display::Tone::Error, 4000);
    return;
  }
  notify(p.name, "Grouping rooms", display::Tone::Busy);
  sonos::group(p.rooms);
  if (p.volume >= 0) sonos::setVolumes(p.rooms, p.volume);
  notify(p.name, "Starting " + p.favourite.title, display::Tone::Busy);
  bool ok = sonos::playFavourite(p.rooms[0], p.favourite, p.shuffle);
  setGroup(p.rooms);
  if (ok) setActivePreset(i);
  notify(p.name, ok ? p.favourite.title : sonos::lastPlayError(),
          ok ? display::Tone::Info : display::Tone::Error, ok ? MESSAGE_MS : 4000);
}

void playRandom() {
  auto all = sonos::favourites();
  std::vector<Favourite> favs;
  for (auto &f : all)
    if (unsupportedReason(f).isEmpty()) favs.push_back(f);
  if (favs.empty()) {
    notify("No playable favourites", "Add some in the Sonos app", display::Tone::Error);
    return;
  }
  int pick = esp_random() % favs.size();
  if (favs.size() > 1 && pick == g_lastRandom) pick = (pick + 1) % favs.size();
  g_lastRandom = pick;

  std::vector<String> grp = currentGroup();
  if (grp.empty()) {  // nothing played yet: use Preset 1's rooms
    grp = presets::get(0).rooms;
    if (grp.empty()) {
      notify("No group yet", "Press a preset first", display::Tone::Error);
      return;
    }
    sonos::group(grp);
    setGroup(grp);
  }
  setActivePreset(RANDOM_BADGE);
  notify("Random favourite", favs[pick].title, display::Tone::Busy);
  bool ok = sonos::playFavourite(sonos::coordinatorOf(grp[0]), favs[pick], false);
  if (!ok) notify("Random favourite", sonos::lastPlayError(), display::Tone::Error, 4000);
}

void handle(Action a) {
  g_lastActivity = millis();
  Serial.printf("[action] %s %u\n", name(a.type), a.index);

  if (a.type == ActionType::Rescan) {
    g_phase = Phase::Discovering;
    notify("Rescanning", "Looking for Sonos players", display::Tone::Busy);
    if (sonos::discover()) {
      sonos::loadFavourites();
      g_phase = Phase::Ready;
      notify("Sonos found", String(sonos::rooms().size()) + " rooms, " + String(sonos::favourites().size()) + " favourites");
      syncServices(false);
    } else {
      g_phase = Phase::NoSonos;
      notify("No Sonos found", "Check the controller is on the same network", display::Tone::Error);
    }
    return;
  }
  if (a.type == ActionType::Sync) {  // works even before Sonos is found
    syncServices(true);
    return;
  }
  if (g_phase != Phase::Ready) {
    notify("Sonos not found", "Still searching the network", display::Tone::Error);
    return;
  }
  if (a.type == ActionType::Preset) return runPreset(a.index);
  if (a.type == ActionType::Random) return playRandom();

  String coord = targetCoordinator();
  if (coord.isEmpty()) {
    notify("No group yet", "Press a preset first", display::Tone::Error);
    return;
  }
  switch (a.type) {
    case ActionType::PlayPause:
      sonos::playPause(coord);
      g_msgUntil = 0;
      g_quickPollAt = millis() + 700;
      break;
    case ActionType::Stop:
      sonos::stop(coord);
      g_msgUntil = 0;
      g_quickPollAt = millis() + 700;
      break;
    case ActionType::Next:
      if (!sonos::next(coord)) notify("Can't skip", "Not supported on this source", display::Tone::Error);
      else {
        g_msgUntil = 0;
        g_quickPollAt = millis() + 700;
      }
      break;
    case ActionType::VolUp:
    case ActionType::VolDown: {
      // Merge queued volume presses into one call (auto-repeat floods the queue).
      int delta = a.type == ActionType::VolUp ? VOLUME_STEP : -VOLUME_STEP;
      Action more;
      while (xQueuePeek(g_queue, &more, 0) == pdTRUE &&
             (more.type == ActionType::VolUp || more.type == ActionType::VolDown)) {
        xQueueReceive(g_queue, &more, 0);
        delta += more.type == ActionType::VolUp ? VOLUME_STEP : -VOLUME_STEP;
      }
      int vol = sonos::adjustGroupVolume(coord, delta);
      if (vol >= 0) {
        g_msgUntil = millis() + 1500;
        display::volume(vol);
        publishVolume(vol);
      }
      break;
    }
    case ActionType::Volume: {
      // A dragged slider sends many; only the latest matters.
      int vol = a.index;
      Action more;
      while (xQueuePeek(g_queue, &more, 0) == pdTRUE && more.type == ActionType::Volume) {
        xQueueReceive(g_queue, &more, 0);
        vol = more.index;
      }
      vol = constrain(vol, 0, 100);
      if (sonos::setGroupVolume(coord, vol)) {
        g_msgUntil = millis() + 1500;
        display::volume(vol);
        publishVolume(vol);
      }
      break;
    }
    default:
      break;
  }
}

// ---- worker ----------------------------------------------------------------

void worker(void *) {
  uint32_t lastDiscover = 0, lastTopology = 0;
  for (;;) {
    if (g_phase != Phase::Ready && WiFi.status() == WL_CONNECTED &&
        (lastDiscover == 0 || millis() - lastDiscover > REDISCOVER_MS)) {
      lastDiscover = millis();
      g_phase = Phase::Discovering;
      notify("Finding Sonos", WiFi.localIP().toString(), display::Tone::Busy, 0);
      if (sonos::discover()) {
        sonos::loadFavourites();
        lastTopology = millis();
        g_phase = Phase::Ready;
        notify("Sonos found", String(sonos::rooms().size()) + " rooms, " + String(sonos::favourites().size()) + " favourites");
      } else {
        g_phase = Phase::NoSonos;
        notify("No Sonos found", "Retrying in 30 seconds", display::Tone::Error, 0);
      }
    }

    Action a;
    // Normally poll every POLL_INTERVAL_MS; sooner right after Play/Pause/Stop/Next,
    // because the player can briefly report the old or a transitional state.
    uint32_t wait = POLL_INTERVAL_MS;
    if (g_quickPollAt) {
      int32_t left = (int32_t)(g_quickPollAt - millis());
      wait = left > 0 ? (uint32_t)left : 0;
      g_quickPollAt = 0;
    }
    if (xQueueReceive(g_queue, &a, pdMS_TO_TICKS(wait)) == pdTRUE) handle(a);

    if (g_phase == Phase::Ready) {
      if (millis() - lastTopology > TOPOLOGY_REFRESH_MS) {
        sonos::refreshTopology();
        lastTopology = millis();
      }
      if (spotify::connected() && (g_lastServices == 0 || millis() - g_lastServices > SERVICES_REFRESH_MS))
        syncServices(false);
      renderNowPlaying();
    }
  }
}

}  // namespace

void begin() {
  g_queue = xQueueCreate(16, sizeof(Action));
  g_mtx = xSemaphoreCreateMutex();
  loadGroup();
  // HTTP(S) + TLS handshakes + string parsing need a roomy stack.
  xTaskCreatePinnedToCore(worker, "sonos", 24576, nullptr, 1, nullptr, 1);
}

bool post(ActionType type, uint8_t index) {
  Action a{type, index};
  return xQueueSend(g_queue, &a, 0) == pdTRUE;
}

const char *name(ActionType t) { return NAMES[(int)t]; }

bool parse(const String &s, ActionType &out) {
  for (int i = 0; i < (int)(sizeof(NAMES) / sizeof(NAMES[0])); i++)
    if (s == NAMES[i]) {
      out = (ActionType)i;
      return true;
    }
  return false;
}

Phase phase() { return g_phase; }

std::vector<String> currentGroup() {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  auto g = g_group;
  xSemaphoreGive(g_mtx);
  return g;
}

uint32_t lastActivity() { return g_lastActivity; }

void nowPlayingJson(JsonObject o) {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  NowState n = g_now;
  xSemaphoreGive(g_mtx);
  o["ok"] = n.ok;
  o["state"] = n.state;
  o["title"] = n.title;
  o["artist"] = n.artist;
  o["album"] = n.album;
  o["art"] = n.art;
  o["group"] = n.group;
  o["badge"] = n.badge;
  o["volume"] = n.volume;
  o["preset"] = n.preset;
}

void onNowPlayingChange(std::function<void()> cb) { g_onNow = cb; }

}  // namespace actions
