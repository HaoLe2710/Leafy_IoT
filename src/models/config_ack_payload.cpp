#include "models/config_ack_payload.h"

#include <ArduinoJson.h>

namespace leafy {

String ConfigAckPayload::buildJson(
    const String& timestampIso,
    uint32_t configVersion,
    bool success,
    const String& errorMessage) {
  JsonDocument doc;

  doc["type"] = "config";
  doc["configVersion"] = configVersion;
  doc["success"] = success;
  if (timestampIso.length() > 0) {
    doc["ts"] = timestampIso;
  }
  if (success) {
    doc["error"] = nullptr;
  } else {
    doc["error"] = errorMessage;
  }

  String output;
  serializeJson(doc, output);
  return output;
}

}  // namespace leafy
