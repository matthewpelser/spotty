# Spotty

ESP32-S3 physical controller for a Sonos system (presets + transport + 240×280 now-playing screen).

**Start by reading [docs/README.md](docs/README.md)** — status, resume checklist, and links to
architecture, decision log, display design, Sonos protocol notes, hardware and backlog.

- Build/flash with `~/.platformio/penv/bin/pio` (not on PATH). Close any open serial monitor before uploading.
- All network I/O and drawing happen in the single worker task (`src/actions.cpp`); handlers only post actions.
- Keep large buffers/JSON in PSRAM — the internal heap must stay free for WiFi (see docs/decisions.md D14).
- Web UI lives in `src/web_ui.h` (one self-contained page); see docs/web-ui.md for screens and how to test it at phone size (headless Chrome via DevTools — desktop Chrome won't shrink below ~500 px).
- Large JSON lists: write the text directly into a PSRAM `String` (see `handleFavourites`), don't build big JsonDocuments (D23).
- Record new decisions in `docs/decisions.md` and update `docs/backlog.md` when things are done or found.
