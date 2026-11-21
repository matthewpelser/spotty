#include "display.h"

#include "art.h"
#include "config.h"

namespace display {

namespace {

// ---- Fixed colours (RGB888); the rest come from the album art palette ------
constexpr uint32_t FG = 0xF4F7F9u;
constexpr uint32_t BAD = 0xF07A70u;
constexpr uint32_t PILL = 0x121C24u;

// Content inset: the Waveshare panel has rounded corners.
constexpr int MX = 22;

// Hero layout geometry.
constexpr int ART_H = 196;     // art occupies the top 196 rows
constexpr int FADE_LEN = 80;   // rows over which art fades into the background
constexpr int CW = WIDTH - 2 * MX;

const lgfx::IFont *F_SMALL = &fonts::FreeSans9pt7b;
const lgfx::IFont *F_SMALL_B = &fonts::FreeSansBold9pt7b;
const lgfx::IFont *F_TITLE = &fonts::FreeSansBold12pt7b;

#if DISPLAY_PANEL
class Panel : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;

 public:
  Panel() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = LCD_PIN_SCLK;
      cfg.pin_mosi = LCD_PIN_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = LCD_PIN_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = LCD_PIN_CS;
      cfg.pin_rst = LCD_PIN_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;  // ST7789 RAM is 240x320; the glass shows 280 rows
      cfg.memory_height = 320;
      cfg.panel_width = WIDTH;
      cfg.panel_height = HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 20;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = LCD_PIN_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};
Panel g_panel;
#endif

lgfx::LGFX_Sprite g_canvas;
uint16_t *g_frame = nullptr;  // published frame, little-endian RGB565
SemaphoreHandle_t g_mtx;
volatile uint32_t g_version = 0;
std::function<void()> g_cb;

NowPlayingView g_view;  // last now-playing view (for volume() redraws)
bool g_haveView = false;
String g_lastKey;

// ---- pixel helpers -------------------------------------------------------------

inline uint16_t swapped565(uint32_t c) {
  uint16_t v = ((c >> 19) & 0x1F) << 11 | ((c >> 10) & 0x3F) << 5 | ((c >> 3) & 0x1F);
  return __builtin_bswap16(v);
}

inline uint16_t blend(uint16_t px, uint32_t c, int a) {  // a: 0..255 of colour c
  uint16_t v = __builtin_bswap16(px);
  int r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
  r += (((int)(c >> 16 & 255) - r) * a) >> 8;
  g += (((int)(c >> 8 & 255) - g) * a) >> 8;
  b += (((int)(c & 255) - b) * a) >> 8;
  return swapped565((uint32_t)r << 16 | g << 8 | b);
}

// Translucent rounded rectangle straight into the framebuffer.
void blendRect(int x0, int y0, int w, int h, uint32_t c, int alpha, int radius) {
  uint16_t *buf = (uint16_t *)g_canvas.getBuffer();
  for (int y = max(0, y0); y < min(HEIGHT, y0 + h); y++) {
    for (int x = max(0, x0); x < min(WIDTH, x0 + w); x++) {
      if (radius) {  // skip pixels outside the rounded corners
        int dx = x < x0 + radius ? x0 + radius - x : (x >= x0 + w - radius ? x - (x0 + w - radius - 1) : 0);
        int dy = y < y0 + radius ? y0 + radius - y : (y >= y0 + h - radius ? y - (y0 + h - radius - 1) : 0);
        if (dx && dy && dx * dx + dy * dy > radius * radius) continue;
      }
      buf[y * WIDTH + x] = blend(buf[y * WIDTH + x], c, alpha);
    }
  }
}

// ---- text helpers --------------------------------------------------------------

// Word-wrap into at most maxLines lines (ellipsis if cut). Returns y below the text.
int drawWrapped(const String &text, int x, int y, int w, int lineH, int maxLines, int align,
                uint32_t color) {
  auto &c = g_canvas;
  c.setTextColor(color);
  c.setTextDatum(align ? lgfx::top_center : lgfx::top_left);
  int ax = align ? x + w / 2 : x;
  String rest = text;
  rest.trim();
  for (int line = 0; line < maxLines && rest.length(); line++) {
    int fit = rest.length();
    while (fit > 0 && c.textWidth(rest.substring(0, fit)) > w) {
      int sp = rest.lastIndexOf(' ', fit - 1);
      fit = (sp > 0 && line < maxLines - 1) ? sp : fit - 1;
    }
    if (fit == 0) fit = 1;
    String part = rest.substring(0, fit);
    rest = rest.substring(fit);
    rest.trim();
    if (line == maxLines - 1 && rest.length()) {
      while (part.length() && c.textWidth(part + "...") > w) part.remove(part.length() - 1);
      part.trim();
      part += "...";
    }
    c.drawString(part, ax, y);
    y += lineH;
  }
  return y;
}

void drawStateIcon(const String &state, int x, int y, uint32_t color) {  // 10x12 box
  auto &c = g_canvas;
  if (state == "PLAYING") {
    c.fillTriangle(x, y, x, y + 11, x + 9, y + 5, color);
  } else if (state == "PAUSED_PLAYBACK") {
    c.fillRect(x, y, 3, 12, color);
    c.fillRect(x + 6, y, 3, 12, color);
  } else if (state == "TRANSITIONING") {
    for (int i = 0; i < 3; i++) c.fillCircle(x + 1 + i * 4, y + 6, 1, color);
  } else {
    c.fillRect(x, y + 1, 10, 10, color);
  }
}

void publish() {
  const uint16_t *src = (const uint16_t *)g_canvas.getBuffer();
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  for (int i = 0; i < WIDTH * HEIGHT; i++) g_frame[i] = __builtin_bswap16(src[i]);
  g_version = g_version + 1;
  xSemaphoreGive(g_mtx);
#if DISPLAY_PANEL
  g_canvas.pushSprite(&g_panel, 0, 0);
#endif
  if (g_cb) g_cb();
}

bool changed(const String &key) {
  if (key == g_lastKey) return false;
  g_lastKey = key;
  return true;
}

// ---- views -----------------------------------------------------------------------

uint32_t mixColor(uint32_t a, uint32_t b, float t) {
  auto ch = [&](int sh) {
    int x = a >> sh & 255, y = b >> sh & 255;
    return (uint32_t)(x + (y - x) * t) & 255;
  };
  return ch(16) << 16 | ch(8) << 8 | ch(0);
}

// Tiny (1/8 scale) cover scaled up with bilinear filtering: a soft-focus
// placeholder while the sharp cover decodes. Returns the bottom row drawn.
int drawSoftPreview(lgfx::LGFX_Sprite &tiny) {
  const int tw = tiny.width(), th = tiny.height();
  if (tw < 2 || th < 2) return 0;
  const uint16_t *src = (const uint16_t *)tiny.getBuffer();
  uint16_t *dst = (uint16_t *)g_canvas.getBuffer();
  const int sh = th * WIDTH / tw;                 // scaled image height
  const int off = max(0, (sh - ART_H) * 3 / 10);  // same crop as the sharp cover
  const int bottom = min(ART_H, sh - off);
  auto rgbAt = [&](int x, int y, int &r, int &g, int &b) {
    uint16_t v = __builtin_bswap16(src[y * tw + x]);
    r = (v >> 11) << 3; g = ((v >> 5) & 63) << 2; b = (v & 31) << 3;
  };
  for (int y = 0; y < bottom; y++) {
    int fy = ((y + off) * 256 + 128) * th / sh - 128;  // 8.8 fixed point
    fy = constrain(fy, 0, (th - 1) * 256 - 1);
    int y0 = fy >> 8, wy = fy & 255;
    for (int x = 0; x < WIDTH; x++) {
      int fx = ((x * 256 + 128) * tw) / WIDTH - 128;
      fx = constrain(fx, 0, (tw - 1) * 256 - 1);
      int x0 = fx >> 8, wx = fx & 255;
      int r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;
      rgbAt(x0, y0, r00, g00, b00);
      rgbAt(x0 + 1, y0, r10, g10, b10);
      rgbAt(x0, y0 + 1, r01, g01, b01);
      rgbAt(x0 + 1, y0 + 1, r11, g11, b11);
      auto lerp2 = [&](int a, int b, int c, int d) {
        int top = a * (256 - wx) + b * wx, bot = c * (256 - wx) + d * wx;
        return (top * (256 - wy) + bot * wy) >> 16;
      };
      dst[y * WIDTH + x] = swapped565((uint32_t)lerp2(r00, r10, r01, r11) << 16 |
                                      lerp2(g00, g10, g01, g11) << 8 | lerp2(b00, b10, b01, b11));
    }
  }
  return bottom;
}

void drawHero() {
  const NowPlayingView &v = g_view;
  const art::Palette pal = art::palette();
  auto &c = g_canvas;
  c.fillScreen(pal.base);

  // Cover across the top, cropped 30% from the top like CSS object-position.
  int artBottom = ART_H;
  art::Stage stage = art::stage();
  if (stage == art::Stage::Full) {
    auto &a = art::full();
    int off = max(0, (a.height() - ART_H) * 3 / 10);
    a.pushSprite(&c, 0, -off);
    artBottom = min(ART_H, a.height() - off);
    c.fillRect(0, artBottom, WIDTH, HEIGHT - artBottom, pal.base);
  } else if (stage == art::Stage::Preview) {
    artBottom = drawSoftPreview(art::preview());
  } else if (stage == art::Stage::Pending) {
    // Not loaded yet: gradient in the (remembered or previous) colours.
    uint16_t *px = (uint16_t *)c.getBuffer();
    uint32_t top = mixColor(pal.base, pal.accent, 0.35f);
    for (int y = 0; y < ART_H; y++) {
      uint16_t col = swapped565(mixColor(top, pal.base, (float)y / ART_H));
      for (int x = 0; x < WIDTH; x++) px[y * WIDTH + x] = col;
    }
  } else {
    // No cover: tinted panel with a music note.
    blendRect(0, 0, WIDTH, ART_H, pal.accent, 30, 0);
    uint32_t note = pal.soft;
    c.fillCircle(104, 104, 13, note);
    c.fillRect(113, 46, 5, 58, note);
    c.fillTriangle(118, 46, 118, 70, 138, 62, note);
  }
  // Fade the bottom of the art into the background.
  uint16_t *buf = (uint16_t *)c.getBuffer();
  int fadeTop = max(0, artBottom - FADE_LEN);
  for (int y = fadeTop; y < artBottom; y++) {
    int t = (y - fadeTop) * 256 / FADE_LEN;
    int a = min(255, t * t / 256 + t / 4);  // ease-in, fully base at the bottom
    for (int x = 0; x < WIDTH; x++) buf[y * WIDTH + x] = blend(buf[y * WIDTH + x], pal.base, a);
  }

  // Preset pill.
  if (v.badge.length()) {
    c.setFont(F_SMALL_B);
    String b = v.badge;
    while (b.length() > 1 && c.textWidth(b) > WIDTH - 2 * MX - 18) b.remove(b.length() - 1);
    int w = c.textWidth(b) + 18;
    blendRect(MX, 14, w, 22, PILL, 190, 11);
    c.setTextDatum(lgfx::middle_left);
    c.setTextColor(FG);
    c.drawString(b, MX + 9, 26);
  }

  const int textW = WIDTH - 2 * MX;
  c.setFont(F_TITLE);
  drawWrapped(v.title.length() ? v.title : String("Nothing playing"), MX, 168, textW, 26, 1, 0,
              v.title.length() ? FG : pal.soft);
  c.setFont(F_SMALL);
  drawWrapped(v.artist, MX, 198, textW, 20, 1, 0, pal.soft);

  // Play state, room, volume number.
  drawStateIcon(v.state, MX, 230, v.state == "PLAYING" ? pal.accent : pal.soft);
  c.setFont(F_SMALL_B);
  String vol = v.volume < 0 ? String("--") : String(v.volume);
  int volW = c.textWidth(vol);
  c.setTextDatum(lgfx::middle_right);
  c.setTextColor(FG);
  c.drawString(vol, WIDTH - MX, 237);
  c.setFont(F_SMALL);
  drawWrapped(v.group, MX + 18, 228, textW - 18 - volW - 10, 18, 1, 0, pal.soft);

  // Volume bar.
  const int bx = 30, bw = WIDTH - 60, by = 254;
  blendRect(bx, by, bw, 4, 0xFFFFFF, 46, 2);
  int fill = v.volume <= 0 ? 0 : max(4, bw * min(v.volume, 100) / 100);
  if (fill) c.fillRoundRect(bx, by, fill, 4, 2, pal.accent);
}

}  // namespace

// ---- public ------------------------------------------------------------------------

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  g_canvas.setPsram(true);
  g_canvas.setColorDepth(16);
  g_canvas.createSprite(WIDTH, HEIGHT);
  g_frame = (uint16_t *)ps_malloc(FRAME_BYTES);
  if (!g_canvas.getBuffer() || !g_frame) Serial.println("[screen] framebuffer alloc failed");
#if DISPLAY_PANEL
  g_panel.init();
  g_panel.setBrightness(LCD_BRIGHTNESS);
  g_panel.fillScreen(TFT_BLACK);
#endif
}

void message(const String &title, const String &detail, Tone tone) {
  String t = ascii(title), d = ascii(detail);
  String header = g_haveView ? g_view.group : String("Spotty");
  art::Palette pal = art::palette();
  if (!changed("M|" + t + "|" + d + "|" + (int)tone + "|" + header + "|" + pal.base)) return;
  Serial.printf("[screen] %s | %s\n", t.c_str(), d.c_str());

  auto &c = g_canvas;
  c.fillScreen(pal.base);
  c.setFont(F_SMALL_B);
  c.setTextDatum(lgfx::top_left);
  c.setTextColor(pal.soft);
  c.drawString(header, MX, 16);
  c.drawFastHLine(MX, 42, WIDTH - 2 * MX, art::palette().soft);
  blendRect(MX, 42, WIDTH - 2 * MX, 1, pal.base, 140, 0);  // soften the rule

  uint32_t color = tone == Tone::Error ? BAD : tone == Tone::Busy ? pal.accent : FG;
  c.setFont(F_TITLE);
  int y = drawWrapped(t, MX, 92, WIDTH - 2 * MX, 28, 2, 1, color);
  c.setFont(F_SMALL);
  y = drawWrapped(d, MX, y + 8, WIDTH - 2 * MX, 22, 3, 1, pal.soft);
  if (tone == Tone::Busy)
    for (int i = 0; i < 3; i++) c.fillCircle(WIDTH / 2 - 14 + i * 14, y + 18, 4, pal.accent);
  publish();
}

String heroKey() {
  const NowPlayingView &v = g_view;
  return "N|" + v.title + "|" + v.artist + "|" + v.state + "|" + v.volume + "|" + v.group + "|" +
         v.badge + "|" + v.artUrl + "|" + (int)art::stage();
}

void nowPlaying(const NowPlayingView &in) {
  NowPlayingView v = in;
  v.group = ascii(v.group);
  v.badge = ascii(v.badge);
  v.title = ascii(v.title);
  v.artist = ascii(v.artist);
  art::select(v.artUrl, v.albumKey);  // instant: prefetched cover or remembered colours
  g_view = v;
  g_haveView = true;
  if (changed(heroKey())) {  // text first, before any download
    Serial.printf("[screen] %s - %s [%s %d]\n", v.title.c_str(), v.artist.c_str(), v.state.c_str(),
                  v.volume);
    drawHero();
    publish();
  }
  while (art::step()) {  // soft preview, then sharp cover
    g_lastKey = heroKey();
    drawHero();
    publish();
  }
}

void volume(int vol) {
  if (!g_haveView) {
    message("Volume " + String(vol), "");
    return;
  }
  g_view.volume = vol;
  g_lastKey = "";  // force redraw
  drawHero();
  publish();
}

size_t copyFrame(uint8_t *dst, size_t maxLen, size_t index) {
  if (!g_frame || index >= FRAME_BYTES) return 0;
  size_t n = min(maxLen, FRAME_BYTES - index);
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  memcpy(dst, (const uint8_t *)g_frame + index, n);
  xSemaphoreGive(g_mtx);
  return n;
}

uint32_t frameVersion() { return g_version; }

void onChange(std::function<void()> cb) { g_cb = cb; }

String ascii(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length();) {
    uint8_t c = s[i];
    uint32_t cp;
    int len;
    if (c < 0x80) { cp = c; len = 1; }
    else if ((c >> 5) == 0x6) { cp = c & 0x1F; len = 2; }
    else if ((c >> 4) == 0xE) { cp = c & 0x0F; len = 3; }
    else if ((c >> 3) == 0x1E) { cp = c & 0x07; len = 4; }
    else { i++; continue; }
    for (int k = 1; k < len && i + k < s.length(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
    i += len;

    if (cp >= 0x20 && cp < 0x7F) { out += (char)cp; continue; }
    if (cp < 0x20) { out += ' '; continue; }
    // Latin-1 supplement letters -> base letters.
    static const char *latin1 =
        "AAAAAAACEEEEIIII"   // C0-CF
        "DNOOOOOxOUUUUYTs"   // D0-DF
        "aaaaaaaceeeeiiii"   // E0-EF
        "dnooooo/ouuuuyty";  // F0-FF
    if (cp >= 0xC0 && cp <= 0xFF) {
      if (cp == 0xC6) out += "AE";
      else if (cp == 0xE6) out += "ae";
      else if (cp == 0xDF) out += "ss";
      else if (cp == 0xDE) out += "Th";
      else if (cp == 0xFE) out += "th";
      else out += latin1[cp - 0xC0];
      continue;
    }
    switch (cp) {
      case 0x2018: case 0x2019: case 0x00B4: out += '\''; break;
      case 0x201C: case 0x201D: out += '"'; break;
      case 0x2013: case 0x2014: out += '-'; break;
      case 0x2026: out += "..."; break;
      case 0x00A0: out += ' '; break;
      case 0x0152: out += "OE"; break;
      case 0x0153: out += "oe"; break;
      default: out += '?';
    }
  }
  return out;
}

}  // namespace display
