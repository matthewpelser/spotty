# Spotty

A small physical controller for a **Sonos** system, built on a Waveshare **ESP32-S3-Nano**.
Press a button to start a scene — *"Kitchen and Living Room, volume 40%, play my Chill playlist"* —
or use Play/Pause, Stop, Next, Volume and Random. It talks to the speakers directly over the local
network (UPnP), with no cloud account needed.

- **4 presets** (rooms + volume + a Sonos Favourite or a Spotify playlist/album, optional shuffle) and transport buttons
- **240×280 colour screen** ("Hero fade"): album art with colours taken from the cover, preset pill, title, room and volume
- **Web app** at `http://spotty.local`, designed for phones: live copy of the screen, preset tiles, volume slider,
  preset editor with a searchable music picker, controller settings
- **Optional Spotify sign-in** to pick from your own playlists and saved albums (playback still goes through Sonos)

## Build and flash

[PlatformIO](https://platformio.org/) with the `espressif32` platform (Arduino core 2.0.17):

```sh
pio run                 # build
pio run -t upload       # flash (close any open serial monitor first)
pio device monitor      # logs at 115200
```

On first boot, join the **Spotty-Setup** Wi-Fi network to give the controller your Wi-Fi details,
then open `http://spotty.local`.

## Documentation

Design notes, decisions and protocol details are in [docs/](docs/README.md):
architecture, decision log, LCD design, web app design, Sonos notes, hardware/pins and the backlog.

## Status

Working proof of concept on a real 3-room Sonos system. Physical buttons and the LCD panel are
coded but not yet wired; TuneIn and Sonos Radio favourites can't be started over local control
(see [docs/decisions.md](docs/decisions.md), D9).
