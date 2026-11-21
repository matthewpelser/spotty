# Hardware

## Board

Waveshare **ESP32-S3-Nano** (ESP32-S3R8): 16 MB flash, 8 MB octal PSRAM, Arduino Nano pinout.
PlatformIO env `waveshare_esp32s3_nano` (`board = esp32-s3-devkitc-1`, `qio_opi`, `default_16MB.csv`, LittleFS).
USB: built-in USB-Serial/JTAG (`/dev/cu.usbmodem101` on the Mac), CDC on boot.

## Buttons (not yet wired)

Momentary switch between pin and **GND**; `INPUT_PULLUP`, active low, 25 ms debounce.
Pin numbers are Nano labels → ESP32-S3 GPIO; **check against the Waveshare pinout before soldering.**

| Button | Nano | GPIO |
|---|---|---|
| Preset 1–4 | D2–D5 | 5, 6, 7, 8 |
| Play/Pause | D6 | 9 |
| Stop | D7 | 10 |
| Vol − | D8 | 17 |
| Vol + | D9 | 18 |
| Next | D10 | 21 |
| Random favourite | D11 | 38 |
| Status LED (onboard) | D13 | 48 |

LED: fast blink = WiFi setup portal, slow blink = connecting / finding Sonos, solid = ready, short off-flicker = command sent.

## Display (not yet connected)

Waveshare 240×280 ST7789 module (sold as 1.69"; user described it as 1.5"). Rounded corners.
Enable with `DISPLAY_PANEL 1` in `config.h` (default 0 = web preview only). Compiles; **untested on hardware.**

| LCD | Nano | GPIO |
|---|---|---|
| DIN (MOSI) | A0 | 1 |
| CLK | A1 | 2 |
| DC | A3 | 4 |
| CS | A6 | 13 |
| RST | A4 | 11 |
| BL (PWM) | A5 | 12 |
| VCC / GND | 3.3 V / GND | — |

Panel config (`display.cpp`): SPI2, 40 MHz, `memory 240×320`, `panel 240×280`, `offset_y 20`, `invert true`.
If colours/offset look wrong on the real panel, check `invert`, `rgb_order` and `offset_y` first.
