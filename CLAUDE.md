# Spotty

ESP32-S3 physical controller for a Sonos system (presets + transport + 240×280 now-playing screen).

**Start by reading [docs/README.md](docs/README.md)** — status, resume checklist, and links to
architecture, decision log, display design, Sonos protocol notes, hardware and backlog.

- Build/flash with `~/.platformio/penv/bin/pio` (not on PATH). Close any open serial monitor before uploading.
- All network I/O and drawing happen in the single worker task (`src/actions.cpp`); handlers only post actions.
- Keep large buffers/JSON in PSRAM — the internal heap must stay free for WiFi (see docs/decisions.md D14).
- Record new decisions in `docs/decisions.md` and update `docs/backlog.md` when things are done or found.
