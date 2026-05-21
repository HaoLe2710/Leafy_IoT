#include "models/telemetry_payload.h"

#include <ArduinoJson.h>

namespace leafy {

String TelemetryPayload::buildJson(
    const String& timestampIso,
    const String& firmwareVersion,
    int batteryPercent,
    int wifiRssi,
    const std::vector<SensorReading>& readings) {
  JsonDocument doc;

  if (timestampIso.length() > 0) {
    doc["ts"] = timestampIso;
  }
  doc["firmwareVersion"] = firmwareVersion;
  if (batteryPercent >= 0) {
    doc["battery"] = batteryPercent;
  }
  doc["rssi"] = wifiRssi;

  JsonObject metrics = doc["metrics"].to<JsonObject>();
  for (const SensorReading& reading : readings) {
    if (reading.valid && reading.metricCode.length() > 0) {
      metrics[reading.metricCode] = reading.value;
    }
  }

  String output;
  serializeJson(doc, output);
  return output;
}

}  // namespace leafy
