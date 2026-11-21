// Spotty: a physical "favourites" controller for Sonos.
// Buttons fire presets (rooms + volume + Sonos Favourite) and basic transport
// controls over the local network. Configure at http://spotty.local
#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "actions.h"
#include "art.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "netwatch.h"
#include "presets.h"
#include "services/spotify.h"
#include "sonos/sonos.h"
#include "web.h"
#include "wifi_setup.h"

namespace {

enum class LedMode : uint8_t { Portal, Connecting, Searching, Ready };
volatile LedMode g_led = LedMode::Connecting;

// Onboard LED: fast blink = WiFi setup portal, slow blink = connecting / finding
// Sonos, solid = ready, brief off-flicker = command sent.
void ledTask(void *) {
  pinMode(PIN_STATUS_LED, OUTPUT);
  for (;;) {
    uint32_t now = millis();
    LedMode mode = g_led;
    if (mode == LedMode::Ready) {
      if (actions::phase() != Phase::Ready) mode = LedMode::Searching;
      else if (WiFi.status() != WL_CONNECTED) mode = LedMode::Connecting;
    }
    bool on;
    switch (mode) {
      case LedMode::Portal: on = (now / 100) % 2; break;
      case LedMode::Connecting:
      case LedMode::Searching: on = (now / 600) % 2; break;
      default: on = now - actions::lastActivity() > 150; break;
    }
    digitalWrite(PIN_STATUS_LED, on);
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[spotty] booting");

  xTaskCreate(ledTask, "led", 2048, nullptr, 1, nullptr);
  art::begin();
  display::begin();
  display::message("Spotty", "Connecting to WiFi", display::Tone::Busy);

  if (!LittleFS.begin(true)) Serial.println("[fs] LittleFS mount failed");
  presets::begin();
  sonos::begin();
  spotify::begin();

  wifi_setup::connect([] {
    g_led = LedMode::Portal;
    display::message("WiFi setup", String("Join the \"") + SETUP_AP_NAME + "\" network to configure");
  });

  // Log WiFi link events so drop-outs are visible in the serial monitor.
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.printf("[wifi] disconnected, reason %d\n", info.wifi_sta_disconnected.reason);
        break;
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.printf("[wifi] associated, channel %d\n", info.wifi_sta_connected.channel);
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[wifi] got IP %s\n", WiFi.localIP().toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Serial.println("[wifi] lost IP");
        break;
      default:
        break;
    }
  });
  netwatch::begin();

  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  web::begin();
  actions::begin();  // starts Sonos discovery + worker
  buttons::begin();
  g_led = LedMode::Ready;
  Serial.printf("[spotty] ready: http://%s.local  (%s)\n", HOSTNAME,
                WiFi.localIP().toString().c_str());
}

void loop() {
  buttons::update();

  // Network heartbeat for diagnosing connectivity from the serial monitor.
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.printf("[net] status=%d mode=%d bssid=%s ip=%s gw=%s mask=%s rssi=%d ch=%d sleep=%d heap=%u phase=%d up=%lus\n",
                  WiFi.status(), WiFi.getMode(), WiFi.BSSIDstr().c_str(), WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
                  WiFi.RSSI(), WiFi.channel(), WiFi.getSleep(), ESP.getFreeHeap(),
                  (int)actions::phase(), millis() / 1000);
  }
  delay(5);
}
