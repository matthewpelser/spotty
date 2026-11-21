#pragma once
#include <functional>

// Connect to saved WiFi, or open the "Spotty-Setup" captive portal to configure it.
// Kept in its own translation unit: WiFiManager pulls in the synchronous WebServer
// headers, which clash with ESPAsyncWebServer's.
namespace wifi_setup {
// onPortal is called when the setup access point opens.
void connect(std::function<void()> onPortal);
}
