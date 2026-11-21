# Architecture

## Runtime model

```
 buttons (loop, 5 ms) ─┐
 web page (async_tcp) ─┼─► action queue ─► worker task "sonos" ─► Sonos players (UPnP :1400)
                       │                     │   polls now-playing every 2 s
                       │                     ├─► display (framebuffer) ─► web preview / LCD
                       │                     ├─► art (download + decode covers)
                       │                     └─► spotify (HTTPS lists, every 30 min)
 netwatch task (15 s) ── probes router/Sonos, reconnects WiFi or restarts
 led task (25 ms) ────── onboard LED status
```

**Rule: all network I/O and all drawing happen in the single worker task**
(`actions.cpp`). Buttons and HTTP handlers only post `Action`s to a FreeRTOS
queue. This keeps the web server and buttons responsive and avoids locking
around SOAP/HTTP clients. Shared data read by HTTP handlers (rooms, favourites,
presets, Spotify items, frame) is copied under small mutexes.

Worker stack is 24 KB (TLS handshakes). Large JSON lives in PSRAM
(`psram_json.h`) — the internal heap is reserved for WiFi/lwIP (see decision D14).

## Modules (`src/`)

| File | Responsibility |
|---|---|
| `main.cpp` | Boot order: art → display → LittleFS → presets → sonos → spotify → WiFi → WiFi event log → netwatch → mDNS → web → actions → buttons. `loop()`: buttons + `[net]` heartbeat |
| `config.h` | Pins, timings, hostname, `DISPLAY_PANEL` flag, LCD pins |
| `actions.{h,cpp}` | `ActionType` enum, queue, worker loop, preset/random/transport logic, now-playing render, current group + active preset (NVS), prefetch trigger |
| `buttons.{h,cpp}` | Bounce2 per pin → actions. Vol± auto-repeat |
| `sonos/soap.{h,cpp}` | Minimal SOAP client (`HTTPClient`), XML escape/unescape (UTF-8 aware), tag/attr extraction, `lastError()` |
| `sonos/sonos.{h,cpp}` | Discovery (SSDP + topology), favourites, grouping, volume, play favourite, transport, now-playing (incl. art URL/album), `nextArtUrl()`, `spotifyAccount()`, `unsupportedReason()` |
| `catalog.{h,cpp}` | Everything playable = Sonos Favourites + Spotify items. Builds Sonos URIs/DIDL for Spotify items lazily (`forEach`, `find`) |
| `services/spotify.{h,cpp}` | Optional Spotify sign-in (PKCE), token refresh, lists: your playlists, saved albums, new releases (403 for new apps) |
| `services/spotify_certs.h` | Pinned root CAs (DigiCert G2, DigiCert Global Root CA, Starfield G2) |
| `presets.{h,cpp}` | 4 presets in `/presets.json` (LittleFS); favourite metadata resolved via catalog |
| `display.{h,cpp}` | 240×280 framebuffer (LovyanGFX sprite in PSRAM); views: hero now-playing, message; publishes LE RGB565 frame; optional ST7789 panel |
| `art.{h,cpp}` | Cover download (de-chunking), 1/8 preview decode, full decode, palette, album colour cache (NVS), 18-slot LRU cover cache, prefetch |
| `web.{h,cpp}`, `web_ui.h` | ESPAsyncWebServer routes, SSE `/events`, embedded single-page UI |
| `wifi_setup.{h,cpp}` | WiFiManager captive portal (own TU: header clash with async server), forces STA mode after portal |
| `netwatch.{h,cpp}` | Silent-drop watchdog (D15) |
| `psram_json.h` | ArduinoJson allocator that uses PSRAM |

## Persistent state

| Where | Key | Content |
|---|---|---|
| LittleFS | `/presets.json` | 4 presets incl. favourite URI + DIDL metadata |
| NVS `sonos` | `ip` | last player that answered topology |
| NVS `spotty` | `group`, `active` | current group (CSV of UUIDs), active preset index (100 = Random) |
| NVS `spotify` | `client`, `refresh` | Spotify client ID, refresh token (rotates) |
| NVS `art` | `pal`, `next` | 64-entry album colour cache |
| WiFi NVS | — | credentials saved by WiFiManager |

## HTTP API (port 80)

| Method | Path | Notes |
|---|---|---|
| GET | `/` | UI (`web_ui.h`) |
| GET | `/api/state` | phase, WiFi, counts, current group names, `frame` version |
| GET | `/api/rooms` | name, uuid, coordinator |
| GET | `/api/favourites` | catalog: title, uri, group, `unsupported` reason |
| GET/POST | `/api/presets` | array of 4; POST body JSON |
| POST | `/api/action?type=…&n=…` | `preset playpause stop volup voldown next random rescan sync` |
| GET | `/api/screen` | raw LE RGB565 240×280 (snapshot per request) |
| GET | `/events` | SSE `frame` events `{"v":N}` (needs `Accept: text/event-stream`) |
| GET | `/api/services` | Spotify status |
| POST | `/api/services/spotify/{client,connect,callback,disconnect}` | sign-in flow (form params) |

## Libraries

WiFiManager 2.0.17, ESP32Async AsyncTCP + ESPAsyncWebServer 3.x, ArduinoJson 7,
Bounce2, LovyanGFX 1.1.x. Arduino core 2.0.17 (mbedTLS 2.x — `mbedtls_sha256_ret`).
