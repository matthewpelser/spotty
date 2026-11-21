#pragma once
#include <Arduino.h>

// ---- Identity ---------------------------------------------------------------
constexpr const char *HOSTNAME = "spotty";          // http://spotty.local
constexpr const char *SETUP_AP_NAME = "Spotty-Setup";

// ---- Pins (Waveshare ESP32-S3-Nano, Arduino Nano layout) --------------------
// All buttons: momentary switch between pin and GND, INPUT_PULLUP, active low.
// Verify against the Waveshare pinout before soldering.
constexpr uint8_t PIN_PRESET[4] = {5, 6, 7, 8};  // D2..D5
constexpr uint8_t PIN_PLAYPAUSE = 9;             // D6
constexpr uint8_t PIN_STOP = 10;                 // D7
constexpr uint8_t PIN_VOL_DOWN = 17;             // D8
constexpr uint8_t PIN_VOL_UP = 18;               // D9
constexpr uint8_t PIN_NEXT = 21;                 // D10
constexpr uint8_t PIN_RANDOM = 38;               // D11
constexpr uint8_t PIN_STATUS_LED = 48;           // D13 onboard LED

// ---- Behaviour --------------------------------------------------------------
constexpr int NUM_PRESETS = 4;
constexpr uint16_t DEBOUNCE_MS = 25;
constexpr uint16_t VOLUME_REPEAT_MS = 300;   // auto-repeat while Vol± held
constexpr int VOLUME_STEP = 5;
constexpr uint32_t POLL_INTERVAL_MS = 2000;  // now-playing refresh
constexpr uint16_t SOAP_TIMEOUT_MS = 4000;

// ---- Display ---------------------------------------------------------------
// Waveshare 240x280 ST7789 LCD module. The screen is always rendered and shown
// on the web page; set DISPLAY_PANEL to 1 once the module is wired to also
// drive it. Pins below are suggestions on the Nano's free analog header.
#ifndef DISPLAY_PANEL
#define DISPLAY_PANEL 0
#endif
constexpr int8_t LCD_PIN_MOSI = 1;   // A0 -> DIN
constexpr int8_t LCD_PIN_SCLK = 2;   // A1 -> CLK
constexpr int8_t LCD_PIN_DC = 4;     // A3 -> DC
constexpr int8_t LCD_PIN_CS = 13;    // A6 -> CS
constexpr int8_t LCD_PIN_RST = 11;   // A4 -> RST
constexpr int8_t LCD_PIN_BL = 12;    // A5 -> BL (backlight, PWM)
constexpr uint8_t LCD_BRIGHTNESS = 200;
