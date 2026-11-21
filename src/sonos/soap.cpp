#include "soap.h"

#include <HTTPClient.h>

#include "../config.h"

namespace soap {

const Service AVTransport{"/MediaRenderer/AVTransport/Control",
                          "urn:schemas-upnp-org:service:AVTransport:1"};
const Service RenderingControl{"/MediaRenderer/RenderingControl/Control",
                               "urn:schemas-upnp-org:service:RenderingControl:1"};
const Service GroupRenderingControl{"/MediaRenderer/GroupRenderingControl/Control",
                                    "urn:schemas-upnp-org:service:GroupRenderingControl:1"};
const Service ContentDirectory{"/MediaServer/ContentDirectory/Control",
                               "urn:schemas-upnp-org:service:ContentDirectory:1"};
const Service ZoneGroupTopology{"/ZoneGroupTopology/Control",
                                "urn:schemas-upnp-org:service:ZoneGroupTopology:1"};

static int g_lastError = 0;
int lastError() { return g_lastError; }

bool call(const String &ip, const Service &svc, const char *action, const String &args,
          String *response) {
  g_lastError = 0;
  String body;
  body.reserve(args.length() + 400);
  body += F("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
            "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:");
  body += action;
  body += F(" xmlns:u=\"");
  body += svc.urn;
  body += F("\">");
  body += args;
  body += F("</u:");
  body += action;
  body += F("></s:Body></s:Envelope>");

  HTTPClient http;
  http.setTimeout(SOAP_TIMEOUT_MS);
  http.setConnectTimeout(SOAP_TIMEOUT_MS);
  if (!http.begin("http://" + ip + ":1400" + svc.path)) return false;
  http.addHeader("Content-Type", "text/xml; charset=\"utf-8\"");
  http.addHeader("SOAPACTION", String("\"") + svc.urn + "#" + action + "\"");

  int code = http.POST(body);
  bool ok = code == 200;
  if (response || !ok) {
    String resp = code > 0 ? http.getString() : String();
    if (!ok) {
      String err = tag(resp, "errorCode");
      g_lastError = err.length() ? err.toInt() : code;
      Serial.printf("[soap] %s %s -> %d %s\n", ip.c_str(), action, code, err.c_str());
    }
    if (response) *response = std::move(resp);
  }
  http.end();
  return ok;
}

String escape(const String &s) {
  String out;
  out.reserve(s.length() + s.length() / 8 + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&apos;"); break;
      default: out += c;
    }
  }
  return out;
}

static void appendUtf8(String &out, uint32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

String unescape(const String &s) {
  if (s.indexOf('&') < 0) return s;
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c != '&') {
      out += c;
      continue;
    }
    int semi = s.indexOf(';', i);
    if (semi < 0 || semi - (int)i > 8) {
      out += c;
      continue;
    }
    String ent = s.substring(i + 1, semi);
    if (ent == "amp") out += '&';
    else if (ent == "lt") out += '<';
    else if (ent == "gt") out += '>';
    else if (ent == "quot") out += '"';
    else if (ent == "apos") out += '\'';
    else if (ent.startsWith("#x")) appendUtf8(out, strtoul(ent.c_str() + 2, nullptr, 16));
    else if (ent.startsWith("#")) appendUtf8(out, strtoul(ent.c_str() + 1, nullptr, 10));
    else {
      out += c;
      continue;
    }
    i = semi;
  }
  return out;
}

String tag(const String &xml, const char *name, int from, int *endPos) {
  String open = String("<") + name;
  String close = String("</") + name + ">";
  int pos = from;
  while (true) {
    int start = xml.indexOf(open, pos);
    if (start < 0) break;
    // Make sure we matched the whole tag name (<res> not <resMD>).
    char next = xml[start + open.length()];
    if (next != '>' && next != ' ' && next != '/') {
      pos = start + 1;
      continue;
    }
    int gt = xml.indexOf('>', start);
    if (gt < 0) break;
    if (xml[gt - 1] == '/') {  // self-closing: empty content
      if (endPos) *endPos = gt + 1;
      return String();
    }
    int end = xml.indexOf(close, gt);
    if (end < 0) break;
    if (endPos) *endPos = end + close.length();
    return xml.substring(gt + 1, end);
  }
  if (endPos) *endPos = -1;
  return String();
}

String attr(const String &el, const char *name) {
  String key = String(" ") + name + "=\"";
  int p = el.indexOf(key);
  if (p < 0) return String();
  p += key.length();
  int e = el.indexOf('"', p);
  return e < 0 ? String() : el.substring(p, e);
}

}  // namespace soap
