#include "netwatch.h"

#include <WiFi.h>

#include "sonos/sonos.h"

namespace netwatch {

namespace {

constexpr uint32_t CHECK_MS = 15000;
constexpr int RECONNECT_AFTER = 2;  // failed checks (~30 s)
constexpr int RESTART_AFTER = 6;    // failed checks (~90 s)

bool probe(const IPAddress &ip, uint16_t port) {
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  WiFiClient c;
  bool ok = c.connect(ip, port, 2000);
  c.stop();
  return ok;
}

bool reachable() {
  IPAddress gw = WiFi.gatewayIP();
  if (probe(gw, 80) || probe(gw, 53)) return true;
  auto rooms = sonos::rooms();
  IPAddress sonosIp;
  return !rooms.empty() && sonosIp.fromString(rooms[0].ip) && probe(sonosIp, 1400);
}

void task(void *) {
  int failures = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(CHECK_MS));
    if (WiFi.status() != WL_CONNECTED) {  // normal auto-reconnect is handling it
      failures = 0;
      continue;
    }
    if (reachable()) {
      if (failures) Serial.printf("[netwatch] network back after %d failed checks\n", failures);
      failures = 0;
      continue;
    }
    failures++;
    Serial.printf("[netwatch] router and Sonos unreachable (%d) bssid=%s rssi=%d\n", failures,
                  WiFi.BSSIDstr().c_str(), WiFi.RSSI());
    if (failures == RECONNECT_AFTER) {
      Serial.println("[netwatch] forcing WiFi reconnect");
      WiFi.disconnect();
      vTaskDelay(pdMS_TO_TICKS(500));
      WiFi.begin();  // saved credentials
    } else if (failures >= RESTART_AFTER) {
      Serial.println("[netwatch] still unreachable, restarting");
      Serial.flush();
      ESP.restart();
    }
  }
}

}  // namespace

void begin() { xTaskCreate(task, "netwatch", 4096, nullptr, 1, nullptr); }

}  // namespace netwatch
