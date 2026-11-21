#include "wifi_setup.h"

#include <WiFiManager.h>

#include "config.h"

namespace wifi_setup {

void connect(std::function<void()> onPortal) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // lower latency for button -> Sonos

  WiFiManager wm;
  wm.setHostname(HOSTNAME);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(0);  // stay in setup until configured
  wm.setAPCallback([onPortal](WiFiManager *) {
    Serial.printf("[wifi] setup portal open: join '%s'\n", SETUP_AP_NAME);
    if (onPortal) onPortal();
  });
  if (!wm.autoConnect(SETUP_AP_NAME)) {
    Serial.println("[wifi] failed, restarting");
    delay(1000);
    ESP.restart();
  }
  // After a first-time setup the portal's access point can stay up (AP+STA),
  // which leaves the board unreachable from the LAN on some mesh routers.
  // Force plain station mode.
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  Serial.printf("[wifi] connected to %s, IP %s\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str());
}

}  // namespace wifi_setup
