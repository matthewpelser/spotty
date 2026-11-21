# Backlog & open items

## Untested

- **Physical buttons** — code done, not wired.
- **Physical LCD** (`DISPLAY_PANEL 1`) — compiles, never run on the panel.
- **Cover cache hit when going back** to a recent track (`[art] cover from cache`) — not exercised yet.
- **No-art fallback screen** and **progressive-JPEG** covers.
- **Spotify album playback** from the Spotify lists (playlists verified via New Music Friday; albums use the same format as the "I Love Bread" favourite).
- **WiFi setup portal path** after the AP-off fix (only one first-time setup has happened).

## Known limitations

- TuneIn and Sonos Radio favourites can't be started locally (D9).
- Spotify "New releases" endpoint is 403 for new developer apps; editorial lists only if followed.
- Prefetch blocks the worker ~1.1–1.6 s once per track; a button pressed then waits.
- Fonts are ASCII only (accents transliterated, other scripts become "?").
- Title is one line with ellipsis.
- `ssl_client … (-76)` log lines when Spotify closes connections are harmless noise.

## Ideas / not chosen yet

- Sonos cloud Control API for TuneIn / Sonos Radio (decision D9, option 1).
- Split prefetch into smaller steps so buttons never wait.
- Cross-fade between screens (art option 7).
- Make "stopped" more distinct (e.g. dim the cover).
- Volume overlay vs. inline bar (currently inline).
- Persist cover cache across reboots (currently RAM only; colours do persist).

## Housekeeping

- First proof of concept committed as `ad92f8e` (branch `poc`), fast-forward merged into `main` and pushed on 2026-11-21.
- Diagnostic logging kept on purpose: `[net]` heartbeat, `[netwatch]`, `[art]`, `[screen]`, `[soap]`, WiFi events.
