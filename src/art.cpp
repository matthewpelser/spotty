#include "art.h"

#include <HTTPClient.h>
#include <Preferences.h>

#include "display.h"

namespace art {

namespace {

constexpr size_t MAX_BYTES = 600 * 1024;
constexpr uint32_t RETRY_MS = 60000;
constexpr int PALETTE_CACHE = 64;

// ---- colour helpers (RGB888) -------------------------------------------------

uint32_t rgb(int r, int g, int b) {
  return (uint32_t)constrain(r, 0, 255) << 16 | (uint32_t)constrain(g, 0, 255) << 8 |
         (uint32_t)constrain(b, 0, 255);
}
int R(uint32_t c) { return c >> 16 & 255; }
int G(uint32_t c) { return c >> 8 & 255; }
int B(uint32_t c) { return c & 255; }
uint32_t mix(uint32_t a, uint32_t b, float t) {
  return rgb(R(a) + (R(b) - R(a)) * t, G(a) + (G(b) - G(a)) * t, B(a) + (B(b) - B(a)) * t);
}
// Same hue, brightest channel scaled to `peak`.
uint32_t withPeak(uint32_t c, int peak) {
  int m = max(max(R(c), G(c)), max(B(c), 1));
  return rgb(R(c) * peak / m, G(c) * peak / m, B(c) * peak / m);
}

// Most characterful colour of the cover: colour buckets weighted towards
// saturated, mid-brightness pixels, so a grey sky with a blue patch reads blue.
Palette paletteOf(lgfx::LGFX_Sprite &img, int step) {
  const int w = img.width(), h = img.height();
  const uint16_t *px = (const uint16_t *)img.getBuffer();
  static uint32_t weight[512];
  static uint32_t sum[512][3];
  memset(weight, 0, sizeof(weight));
  memset(sum, 0, sizeof(sum));
  uint32_t allR = 0, allG = 0, allB = 0, n = 0;

  for (int y = 0; y < h; y += step) {
    for (int x = 0; x < w; x += step) {
      uint16_t v = __builtin_bswap16(px[y * w + x]);  // sprite stores swapped RGB565
      int r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
      allR += r; allG += g; allB += b; n++;
      int mx = max(r, max(g, b)), mn = min(r, min(g, b));
      int lum = (r * 299 + g * 587 + b * 114) / 1000;
      if (lum < 24 || lum > 232) continue;  // near black / white say little about mood
      int sat = mx ? (mx - mn) * 100 / mx : 0;
      int k = (r >> 5) << 6 | (g >> 5) << 3 | (b >> 5);
      uint32_t wgt = 1 + sat * sat / 16;  // strongly favour colourful pixels (1..626)
      weight[k] += wgt;
      sum[k][0] += r * wgt; sum[k][1] += g * wgt; sum[k][2] += b * wgt;
    }
  }
  uint32_t dominant;
  int best = -1;
  for (int k = 0; k < 512; k++)
    if (weight[k] && (best < 0 || weight[k] > weight[best])) best = k;
  if (best >= 0)
    dominant = rgb(sum[best][0] / weight[best], sum[best][1] / weight[best], sum[best][2] / weight[best]);
  else
    dominant = rgb(allR / max(n, 1u), allG / max(n, 1u), allB / max(n, 1u));

  Palette p;
  p.base = withPeak(dominant, 60);                          // deep tint for the background
  p.accent = mix(withPeak(dominant, 255), 0xFFFFFF, 0.45f);  // light tint
  p.soft = mix(p.base, 0xFFFFFF, 0.72f);
  return p;
}

// ---- image helpers -----------------------------------------------------------

// Dimensions of a JPEG (and whether it is progressive, which TJpgDec can't decode).
bool jpegInfo(const uint8_t *d, size_t n, int &w, int &h, bool &progressive) {
  if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
  size_t i = 2;
  while (i + 9 < n) {
    if (d[i] != 0xFF) { i++; continue; }
    uint8_t m = d[i + 1];
    if (m == 0xFF) { i++; continue; }
    if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
    uint16_t len = d[i + 2] << 8 | d[i + 3];
    if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
      h = d[i + 5] << 8 | d[i + 6];
      w = d[i + 7] << 8 | d[i + 8];
      progressive = m == 0xC2 || m == 0xC6 || m == 0xCA || m == 0xCE;
      return true;
    }
    i += 2 + len;
  }
  return false;
}

bool pngInfo(const uint8_t *d, size_t n, int &w, int &h) {
  if (n < 24 || d[0] != 0x89 || d[1] != 'P') return false;
  w = d[16] << 24 | d[17] << 16 | d[18] << 8 | d[19];
  h = d[20] << 24 | d[21] << 16 | d[22] << 8 | d[23];
  return true;
}

// Undo HTTP chunked transfer encoding in place ("<hex len>\r\n<data>\r\n" ... "0\r\n").
// Sonos sends chunked responses even to HTTP/1.0 requests.
bool dechunk(uint8_t *buf, size_t &len) {
  size_t in = 0, out = 0;
  while (in < len) {
    size_t n = 0;
    int digits = 0;
    while (in < len && isxdigit(buf[in])) {
      char ch = buf[in++];
      n = n * 16 + (isdigit(ch) ? ch - '0' : (tolower(ch) - 'a' + 10));
      digits++;
    }
    while (in < len && buf[in] != '\n') in++;  // skip chunk extensions + CRLF
    in++;
    if (!digits) return false;
    if (n == 0) break;
    if (in + n > len) return false;
    memmove(buf + out, buf + in, n);
    out += n;
    in += n + 2;  // data + CRLF
  }
  len = out;
  return out > 0;
}

uint8_t *download(const String &url, size_t &len) {
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  if (!http.begin(url)) return nullptr;
  const char *keys[] = {"Transfer-Encoding"};
  http.collectHeaders(keys, 1);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[art] GET -> %d\n", code);
    http.end();
    return nullptr;
  }
  int total = http.getSize();  // -1 if unknown
  size_t cap = total > 0 ? (size_t)total : MAX_BYTES;
  if (cap > MAX_BYTES) {
    Serial.printf("[art] too big (%d bytes)\n", total);
    http.end();
    return nullptr;
  }
  uint8_t *buf = (uint8_t *)ps_malloc(cap);
  if (!buf) {
    http.end();
    return nullptr;
  }
  WiFiClient *s = http.getStreamPtr();
  size_t got = 0;
  uint32_t deadline = millis() + 8000;
  while (got < cap && (int32_t)(deadline - millis()) > 0) {
    size_t avail = s->available();
    if (!avail) {
      if (!s->connected()) break;
      delay(5);
      continue;
    }
    got += s->readBytes(buf + got, min(avail, cap - got));
  }
  bool chunked = http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
  http.end();
  if (chunked && !dechunk(buf, got)) got = 0;
  if (got == 0 || (total > 0 && !chunked && got != (size_t)total)) {
    free(buf);
    return nullptr;
  }
  len = got;
  return buf;
}


// ---- album colour cache (option 4) ---------------------------------------------

struct CachedPalette {
  uint32_t key;  // FNV-1a of "artist|album"; 0 = empty
  Palette pal;
};
CachedPalette g_cache[PALETTE_CACHE];
int g_cacheNext = 0;

uint32_t fnv1a(const String &s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
  return h ? h : 1;
}

bool cacheGet(const String &key, Palette &out) {
  if (key.length() < 2) return false;
  uint32_t k = fnv1a(key);
  for (auto &e : g_cache)
    if (e.key == k) {
      out = e.pal;
      return true;
    }
  return false;
}

void cachePut(const String &key, const Palette &pal) {
  if (key.length() < 2) return;
  uint32_t k = fnv1a(key);
  CachedPalette *slot = nullptr;
  for (auto &e : g_cache)
    if (e.key == k) slot = &e;
  if (slot && !memcmp(&slot->pal, &pal, sizeof(pal))) return;  // unchanged
  if (!slot) {
    slot = &g_cache[g_cacheNext];
    g_cacheNext = (g_cacheNext + 1) % PALETTE_CACHE;
  }
  slot->key = k;
  slot->pal = pal;
  Preferences p;  // ~1 KB, written once per new album
  p.begin("art", false);
  p.putBytes("pal", g_cache, sizeof(g_cache));
  p.putInt("next", g_cacheNext);
  p.end();
}

// ---- cover cache: current + prefetched + recently shown covers --------------------
// Each slot is ~128 KB of PSRAM (240x240 sharp + 80x80 preview), ~2.3 MB in total.
constexpr int COVER_SLOTS = 18;

struct Slot {
  String url;
  String key;  // album key, for the colour cache
  Stage stage = Stage::None;
  lgfx::LGFX_Sprite tiny, full;
  Palette pal;
  uint8_t *data = nullptr;  // downloaded bytes, kept between the two stages
  size_t len = 0;
  int w = 0, h = 0;
  bool png = false;
  uint32_t failedAt = 0;
  uint32_t lastUsed = 0;  // millis() when last shown or prefetched (LRU)
};
Slot g_slots[COVER_SLOTS];
int g_cur = 0;

void touch(Slot &s) { s.lastUsed = millis() | 1; }

// Slot already holding a ready (fully decoded) cover for url, or -1.
int findReady(const String &url) {
  if (url.isEmpty()) return -1;
  for (int i = 0; i < COVER_SLOTS; i++)
    if (g_slots[i].url == url && g_slots[i].stage == Stage::Full) return i;
  return -1;
}

// Slot to (re)use: an unused one, else the least recently used (never the current).
int victim() {
  int best = -1;
  for (int i = 0; i < COVER_SLOTS; i++) {
    if (i == g_cur) continue;
    if (!g_slots[i].lastUsed) return i;
    if (best < 0 || g_slots[i].lastUsed - millis() < g_slots[best].lastUsed - millis()) best = i;
  }
  return best;
}
Palette g_pal = defaultPalette();

void freeData(Slot &s) {
  if (s.data) free(s.data);
  s.data = nullptr;
  s.len = 0;
}

void reset(Slot &s, const String &url, const String &key) {
  freeData(s);
  s.url = url;
  s.key = key;
  s.stage = url.isEmpty() ? Stage::None : Stage::Pending;
  s.failedAt = 0;
}

bool newSprite(lgfx::LGFX_Sprite &spr, int w, int h) {
  spr.deleteSprite();
  spr.setPsram(true);
  spr.setColorDepth(16);
  return spr.createSprite(w, h);
}

void fail(Slot &s, const char *why) {
  freeData(s);
  s.stage = Stage::Failed;
  s.failedAt = millis();
  Serial.printf("[art] unavailable: %s\n", why);
}

// Pending -> Preview: download, 1/8-scale decode (DC coefficients only, fast), palette.
void loadPreview(Slot &s) {
  uint32_t t0 = millis();
  s.data = download(s.url, s.len);
  if (!s.data) return fail(s, "download");
  bool progressive = false;
  s.png = false;
  if (!jpegInfo(s.data, s.len, s.w, s.h, progressive)) {
    if (!pngInfo(s.data, s.len, s.w, s.h)) return fail(s, "not a JPEG/PNG");
    s.png = true;
  }
  if (progressive) return fail(s, "progressive JPEG");
  if (s.w <= 0 || s.h <= 0) return fail(s, "bad size");

  int tw = max(1, (s.w + 7) / 8), th = max(1, (s.h + 7) / 8);
  if (!newSprite(s.tiny, tw, th)) return fail(s, "memory");
  bool ok = s.png ? s.tiny.drawPng(s.data, s.len, 0, 0, tw, th, 0, 0, 0.125f, 0.125f)
                  : s.tiny.drawJpg(s.data, s.len, 0, 0, tw, th, 0, 0, 0.125f, 0.125f);
  if (!ok) return fail(s, "decode");
  s.pal = paletteOf(s.tiny, 1);
  s.stage = Stage::Preview;
  Serial.printf("[art] preview %dx%d in %lums\n", tw, th, millis() - t0);
}

// Preview -> Full: sharp decode at display width.
void loadFull(Slot &s) {
  uint32_t t0 = millis();
  int sh = min(400, (s.h * display::WIDTH + s.w / 2) / s.w);
  float scale = (float)display::WIDTH / s.w;
  bool ok = newSprite(s.full, display::WIDTH, sh) &&
            (s.png ? s.full.drawPng(s.data, s.len, 0, 0, display::WIDTH, sh, 0, 0, scale, scale)
                   : s.full.drawJpg(s.data, s.len, 0, 0, display::WIDTH, sh, 0, 0, scale, scale));
  freeData(s);
  if (!ok) {  // keep the preview rather than nothing
    Serial.println("[art] full decode failed, keeping preview");
    s.stage = Stage::Full;
    s.full.deleteSprite();
    return;
  }
  s.stage = Stage::Full;
  Serial.printf("[art] full %dx%d in %lums\n", display::WIDTH, sh, millis() - t0);
}

}  // namespace

Palette defaultPalette() { return {0x1D2630u, 0x9CC6DCu, 0xB9C4CCu}; }

void begin() {
  Preferences p;
  p.begin("art", true);
  if (p.getBytesLength("pal") == sizeof(g_cache)) {
    p.getBytes("pal", g_cache, sizeof(g_cache));
    g_cacheNext = p.getInt("next", 0) % PALETTE_CACHE;
  }
  p.end();
}

void select(const String &url, const String &albumKey) {
  Slot &cur = g_slots[g_cur];
  if (cur.url == url) {
    if (cur.stage == Stage::Failed && millis() - cur.failedAt > RETRY_MS) reset(cur, url, albumKey);
    return;
  }
  int hit = findReady(url);
  if (hit >= 0) {  // prefetched, or shown recently
    g_cur = hit;
    Slot &s = g_slots[hit];
    touch(s);
    s.key = albumKey;
    g_pal = s.pal;
    cachePut(albumKey, s.pal);
    Serial.printf("[art] cover from cache (slot %d)\n", hit);
    return;
  }
  g_cur = victim();  // the previous cover stays cached in its slot
  Slot &next = g_slots[g_cur];
  reset(next, url, albumKey);
  touch(next);
  Palette remembered;
  if (cacheGet(albumKey, remembered)) g_pal = remembered;  // right colours instantly
  else if (url.isEmpty()) g_pal = defaultPalette();
  // otherwise keep the previous colours until the preview arrives
}

bool step() {
  Slot &s = g_slots[g_cur];
  if (s.stage == Stage::Pending) {
    loadPreview(s);
    if (s.stage == Stage::Preview) {
      g_pal = s.pal;
      cachePut(s.key, s.pal);
    }
    return true;
  }
  if (s.stage == Stage::Preview) {
    loadFull(s);
    return true;
  }
  return false;
}

void prefetch(const String &url) {
  if (url.isEmpty() || url == g_slots[g_cur].url) return;
  int hit = findReady(url);
  if (hit >= 0) {  // already cached: just keep it from being evicted
    touch(g_slots[hit]);
    return;
  }
  for (auto &slot : g_slots)  // recently failed: don't hammer it
    if (slot.url == url && slot.stage == Stage::Failed && millis() - slot.failedAt < RETRY_MS) return;
  Slot &s = g_slots[victim()];
  reset(s, url, "");
  touch(s);
  uint32_t t0 = millis();
  loadPreview(s);
  if (s.stage == Stage::Preview) loadFull(s);
  Serial.printf("[art] prefetched next cover in %lums (%s), PSRAM free %u KB\n", millis() - t0,
                s.stage == Stage::Full ? "ok" : "failed",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}

Stage stage() {
  const Slot &s = g_slots[g_cur];
  // A "Full" slot whose sharp decode failed still has its preview.
  if (s.stage == Stage::Full && !const_cast<Slot &>(s).full.getBuffer()) return Stage::Preview;
  return s.stage;
}
Palette palette() { return g_pal; }
lgfx::LGFX_Sprite &preview() { return g_slots[g_cur].tiny; }
lgfx::LGFX_Sprite &full() { return g_slots[g_cur].full; }

}  // namespace art
