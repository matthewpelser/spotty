# Decision log

Newest last. Each entry: what was decided, why, and what was rejected. All dated 2026-11-21 unless noted.

## Product scope

**D1. A "favourites" controller, not a full Sonos remote.**
Presets (rooms + volume + something to play) plus Play/Pause, Stop, Next, Vol±, Random.
*Why:* the user wants one-press scenes; the Sonos app covers everything else.

**D2. 4 presets + 6 control buttons = 10 discrete push buttons. No long-press actions.**
One button = one function. Vol± auto-repeat while held is allowed.
*Rejected:* rotary encoder, keypad matrix, long-press secondary functions.

**D3. Transport buttons act on the group the last preset (or Random) played to.**
Persisted in NVS. Follows the room if it's regrouped in the Sonos app (uses that room's current coordinator).
If no group yet, falls back to Preset 1's rooms.
*Rejected:* fixed home room, room-select button.

**D4. Presets are edited on a built-in web page and stored on flash (LittleFS).**
*Rejected:* hard-coded presets, manually uploaded JSON.

**D5. A preset forms *exactly* its group.** First ticked room is the coordinator;
rooms following that coordinator but not in the preset are removed. Volume is set per room (each speaker to N%).

## Talking to Sonos

**D6. Local UPnP/SOAP on port 1400, no Sonos cloud.**
*Why:* no account/OAuth, works offline, fast.
*Cost:* some things are blocked locally (D9). Revisit the cloud API (Sonos Control API) if radio matters.

**D7. Content = Sonos Favourites + (optionally) Spotify lists.** Favourites are replayed with their stored `res` + `r:resMD` unchanged.

**D8. Containers (playlists/albums/tracks) play via the queue; streams via `SetAVTransportURI`.**
Stream detection: URI prefixes (`x-sonosapi-stream:`, `x-sonosapi-radio:`, `x-rincon-mp3radio:`, `x-sonosapi-hls(-static):`, `x-rincon-stream:`, `aac:`, `hls-radio:`) or `audioBroadcast` class.

**D9. TuneIn favourites are marked unsupported (option 3).**
Sonos firmware 27.3 rejects every TuneIn (`sid=333`/`254`) `SetAVTransportURI` with UPnP 402, even unmodified favourites, even from a PC.
TuneIn's public `Tune.ashx` returned geo-restricted/"not compatible" for BBC/WFMU.
*Behaviour:* labelled "can't play (TuneIn)" in the UI, disabled for new selection, skipped by Random, preset shows "TuneIn favourites can't be started…" without regrouping.
*Rejected for now:* Sonos cloud API (option 1, heavy), custom stream URLs (option 2).

## Spotify

**D10. Optional Spotify sign-in only to *list* things; playback still via Sonos.**
Arbitrary Spotify playlist/album IDs play on Sonos by building the same URI + DIDL Sonos uses for its own Spotify favourites
(see sonos-notes.md). Verified with "New Music Friday" (99 tracks queued).
The account tag (`sn`, `SA_RINCON…` desc) is read from any existing Spotify favourite (`/status/accounts` is empty on this firmware).
*User explicitly rejected:* paste-a-link presets.

**D11. Spotify auth = authorization code + PKCE with the user's own developer app; redirect `http://127.0.0.1:8888/callback`; user pastes the redirected URL back.**
*Why:* Spotify only allows https or loopback redirect URIs, so `spotty.local` can't receive it.
Refresh tokens rotate and are saved. TLS uses pinned root CAs (no `setInsecure`).

**D12. Lists fetched:** `/me/playlists` (≤200), `/me/albums` (50), `/browse/new-releases` (returns 403 for new dev apps → silently skipped).
Spotify editorial/algorithmic lists appear only if the user follows them.

**D13. Sonos Favourites and Spotify items share one list, grouped by source** ("Sonos Favourites", "Spotify - Your playlists", "Spotify - Saved albums").
Originally a `<select>`; since D22 a searchable picker sheet with covers and All / Favourites / Playlists / Albums filters.

## Robustness

**D14. Keep big data out of the internal heap.**
Building DIDL for 225 Spotify items per request (~200 KB internal) starved lwIP and took the device off the network.
Now: catalog items are generated one at a time, DIDL only when needed (`catalog::find`), JSON docs/strings in PSRAM.

**D15. Network watchdog (`netwatch`).** The Google mesh can drop the ESP32 silently (status says connected, no traffic).
Every 15 s probe gateway :80/:53 or a Sonos :1400; 2 failures → WiFi reconnect; 6 → restart. WiFi events + BSSID logged.
Also after first-time setup WiFiManager's AP is forced off (STA only).

**D16. Web preview robustness.** `/api/screen` snapshots the frame per request (no tearing). The page aborts a frame fetch after 5 s,
catches up via `frame` version in `/api/state`, and reconnects SSE. After Play/Pause/Stop/Next the worker re-polls after 0.7 s.

## Display

**D17. 240×280 colour LCD (Waveshare ST7789, 1.69").** Replaced the original 16×2 character simulation.
Everything is drawn into a PSRAM framebuffer (LovyanGFX); the web page shows the exact pixels; the panel gets the same frame.

**D18. Layout "Hero fade" (option A of four).** Rejected: B centred card, C text-first, D full art + strip.
The 1–4 preset strip was dropped — only the playing preset is shown (pill with the preset's name, or "Random").
Vol± updates the bar in place (no separate volume screen). Details in display-design.md.

**D19. Colours come from the album art** (dominant colour weighted to saturated pixels; base/accent/soft derived from it).

**D20. Art loading strategy = options 1 + 3 + 4 + 6, then 5.**
1 text first · 3 soft-focus preview from a 1/8-scale decode · 4 album colour cache (64, NVS) · 6 prefetch next queue track · 5 LRU cache of 18 decoded covers.
Rejected: 2 (plain gradient preview — kept only as the "pending" state), 7 cross-fade.

**D21. Messages (grouping, errors) reuse the current art palette as background.**

## Web app

**D22. Web UI redesigned as "Preset tiles" (option B of three).** iOS-style, light/dark from the phone, mobile first.
Rejected: A "Now Playing" (dark, big art) and C "Inset grouped". Shared screens: editor sheet, searchable music picker,
Controller page. The live LCD simulation (true size, crisp, same bezel as the original page) is shown on the Home screen above the presets and on the Controller page.
The page now renders real text/covers from `/api/nowplaying` instead of a picture of the LCD. Details: web-ui.md.

**D23. `/api/favourites` is written as JSON text by hand, not via a JsonDocument.**
With cover URLs added, building the 246-item list in a JsonDocument using `PsramAllocator` produced a deterministic
corrupted/truncated result (39 items, one with a mangled URI). Writing the JSON straight into one PSRAM `String`
fixed it and uses less memory. `PsramAllocator` is still used for Spotify page parsing, where results check out —
treat it as suspect (see backlog).

**D24. Absolute volume** for the slider: `SnapshotGroupVolume` + `SetGroupVolume` (keeps the rooms' balance),
new `volume` action; queued volume actions are coalesced to the latest.
