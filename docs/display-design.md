# Display design — "Hero fade"

Screen: 240×280 px, rounded corners (keep content ≥ 22 px from left/right edges).
Fonts: LovyanGFX FreeSans family (ASCII only; text is transliterated by `display::ascii()`).
Mockups: <https://claude.ai/artifact/L16iiydx9tmnB8j3UkYfq1> (option A is the chosen one).

## Now-playing layout (`display.cpp` → `drawHero`)

| Element | Position / style |
|---|---|
| Cover art | Top `ART_H = 196` rows, scaled to 240 wide, cropped 30% from the top (like CSS `object-position: 50% 30%`) |
| Fade | Last `FADE_LEN = 80` rows of the art blend into `base` (ease-in) |
| Preset pill | x 22, y 14, height 22, radius 11, `#121C24` at ~75% alpha, FreeSansBold9pt white. Text = preset name or "Random"; hidden if none |
| Title | FreeSansBold12pt, white, x 22, top y 168, one line with "..." |
| Artist | FreeSans9pt, `soft`, top y 198, one line |
| Status row | y ≈ 230–237: state icon (10×12) at x 22 in `accent` when playing else `soft`; room/group name FreeSans9pt `soft` at x 40; volume number FreeSansBold9pt white, right-aligned at x 218 |
| Volume bar | x 30–210, y 254, 4 px, radius 2; track = white at 18%; fill = `accent` |

State icons: ▶ triangle = PLAYING, two bars = PAUSED_PLAYBACK, three dots = TRANSITIONING, square = STOPPED.

No-art fallback: `accent`-tinted panel with a music note in `soft`.

## Message screen

Background `base` of the current palette; header = current group (or "Spotty") in `soft` + hairline at y 42;
title FreeSansBold12pt centred at y 92 (Info white, Busy `accent`, Error `#F07A70`); detail FreeSans9pt `soft`; three dots when Busy.

## Palette (`art.cpp` → `paletteOf`)

- Sample the cover (the 80×80 preview, every pixel). Skip near-black/near-white (luma < 24 or > 232).
- Bucket colours by 3 bits/channel; weight each pixel `1 + sat²/16` (sat 0–100) so colourful areas beat large grey ones.
- `dominant` = average colour of the heaviest bucket (fallback: overall average).
- `base` = dominant with its brightest channel scaled to 60 (deep tint).
- `accent` = dominant scaled to peak 255, mixed 45% toward white.
- `soft` = `base` mixed 72% toward white.
- Default (no art): base `#1D2630`, accent `#9CC6DC`, soft `#B9C4CC`.

Example — "Holy Ghost" (Modern Baseball): dominant `#457888` → base `#1E343C`, accent `#B9EEFF`.

## Art loading stages (`art.cpp`)

| Stage | When | Screen |
|---|---|---|
| select | instantly on track change | prefetched/cached cover → **Full** immediately; else **Pending** with album's remembered colours (or previous colours) |
| Pending | before download | text drawn + vertical gradient (`base`+35% `accent` → `base`) |
| Preview | download + 1/8 decode (measured 0.7–1.1 s incl. download) | soft-focus: 80×80 bilinear-upscaled; palette applied + cached |
| Full | +0.3–0.55 s | sharp 240-wide cover |

- Prefetch: once per track while PLAYING from the queue (not shuffled), look up queue item `Track` (= next, 0-based) via `Browse Q:0` and fully load its cover (~1.1–1.6 s, blocks the worker).
- Cover cache: 18 slots × ~128 KB PSRAM (current + prefetched + last 16), LRU.
- Album colour cache: 64 entries keyed by FNV-1a of `artist|album`, persisted in NVS `art/pal`.
- Progressive JPEGs are not decodable (TJpgDec) → treated as no art.

## Frame pipeline

LovyanGFX sprite (16-bit, byte-swapped RGB565) → `publish()` copies to a little-endian buffer under a mutex,
bumps `frameVersion`, sends SSE `frame`, and (if `DISPLAY_PANEL`) pushes to the ST7789.
Redraws are skipped when the view key (title, artist, state, volume, group, badge, art URL, art stage) is unchanged.
