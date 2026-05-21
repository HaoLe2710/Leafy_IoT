#pragma once

#include <Arduino.h>
#include <vector>

#include "app/sensor_manager.h"

namespace leafy {

struct TelemetryPayload {
  static String buildJson(
      const String& timestampIso,
      const String& firmwareVersion,
      int batteryPercent,
      int wifiRssi,
      const std::vector<SensorReading>& readings);
};

}  // namespace leafy
