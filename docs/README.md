# Spotty — design & decision record

Spotty is a small WiFi box with physical buttons that drives a Sonos system over
the local network. It deliberately does **not** expose every Sonos feature: it
fires a few "scenes" (presets) and basic transport controls, and shows what's
playing on a 240×280 colour screen.

> Example preset: *"All rooms, volume 50%, play Morning Jazz."*

## Where to look

| File | What's in it |
|---|---|
| [architecture.md](architecture.md) | Modules, tasks, data flow, HTTP API |
| [decisions.md](decisions.md) | Decision log: what was chosen, why, what was rejected |
| [display-design.md](display-design.md) | Screen layout ("Hero fade"), colours, art loading stages |
| [sonos-notes.md](sonos-notes.md) | Sonos UPnP/SOAP facts and quirks discovered on the real system |
| [hardware.md](hardware.md) | Board, pins, buttons, LCD wiring |
| [backlog.md](backlog.md) | Open items, untested paths, ideas |

## Current status (2026-11-21)

Working on the real Sonos system (3 rooms: Bedroom, Kitchen, Living Room):

- Presets 1–4: group rooms → set volume → play a favourite (Sonos Favourites or Spotify playlists/albums), optional shuffle.
- Transport: Play/Pause, Stop, Next, Vol±, Random favourite — act on the group the last preset played to.
- Web page at `http://spotty.local`: live screen preview, on-screen copies of all buttons, preset editor, Spotify sign-in.
- Screen: "Hero fade" layout with album art, colours derived from the cover, staged loading, prefetch + cover cache.
- Spotify sign-in (optional) lists the user's playlists and saved albums; playback still goes through Sonos.
- Network watchdog recovers from silent WiFi drop-outs on the Google mesh.

Not yet done: physical buttons are coded but not wired; physical LCD is coded
(`DISPLAY_PANEL`) but not connected. See [backlog.md](backlog.md).

## Resuming work — quick checklist

1. Build: `~/.platformio/penv/bin/pio run` (pio is not on PATH).
2. Flash: `~/.platformio/penv/bin/pio run -t upload` — close any open `pio device monitor` first (it holds the USB port).
3. Logs: `~/.platformio/penv/bin/pio device monitor` (115200). A `[net]` heartbeat prints every 10 s.
4. Screen without a browser: `curl http://spotty.local/api/screen` returns raw little-endian RGB565, 240×280.
5. The device was last seen at `192.168.86.93` on the "Moon" network (Google mesh, `192.168.86.0/24`); `spotty.local` resolves via mDNS.
6. Design mockups (private Artifact): <https://claude.ai/artifact/L16iiydx9tmnB8j3UkYfq1> — option A "Hero fade" was chosen.

Toolchain: PlatformIO, `espressif32` platform, Arduino core **2.0.17**.
