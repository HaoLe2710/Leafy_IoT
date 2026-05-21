#include "models/status_payload.h"

#include <ArduinoJson.h>

namespace leafy {

String StatusPayload::buildJson(
    const String& timestampIso,
    bool online,
    const String& ip,
    const String& wifiSsid,
    int wifiRssi,
    uint64_t uptimeSec) {
  JsonDocument doc;

  if (timestampIso.length() > 0) {
    doc["ts"] = timestampIso;
  }
  doc["online"] = online;
  if (ip.length() > 0) {
    doc["ip"] = ip;
  }
  if (wifiSsid.length() > 0) {
    doc["wifiSsid"] = wifiSsid;
  }
  doc["rssi"] = wifiRssi;
  doc["uptimeSec"] = uptimeSec;

  String output;
  serializeJson(doc, output);
  return output;
}

}  // namespace leafy
