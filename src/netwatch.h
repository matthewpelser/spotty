#pragma once

// Network watchdog. Some mesh routers silently drop a client while the ESP32
// still thinks it's associated (status=connected, but no traffic either way).
// Every 15 s this probes the router and a Sonos player over TCP; if nothing
// answers it forces a WiFi reconnect, and restarts the board if that fails.
namespace netwatch {
void begin();
}
