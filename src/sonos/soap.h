#pragma once
#include <Arduino.h>

// Minimal UPnP/SOAP helpers for talking to Sonos players on port 1400.
namespace soap {

// Sonos service endpoints.
struct Service {
  const char *path;
  const char *urn;
};
extern const Service AVTransport;
extern const Service RenderingControl;
extern const Service GroupRenderingControl;
extern const Service ContentDirectory;
extern const Service ZoneGroupTopology;

// POST a SOAP action. `args` is the inner XML of the action element.
// Returns true on HTTP 200; the raw response body goes to *response if given.
bool call(const String &ip, const Service &svc, const char *action, const String &args,
          String *response = nullptr);

// UPnP errorCode (or HTTP status / negative transport error) of the last failed
// call(); 0 if it succeeded. Only meaningful on the task that made the call.
int lastError();

String escape(const String &s);
String unescape(const String &s);

// Text content of the first <tag ...>...</tag> at or after `from`. Tag names include
// any prefix, e.g. "dc:title". If endPos is given it receives the index after the close tag.
// Returns "" if not found (and sets *endPos to -1).
String tag(const String &xml, const char *name, int from = 0, int *endPos = nullptr);

// Value of attr="..." inside an element's opening-tag text.
String attr(const String &elementText, const char *name);

}  // namespace soap
