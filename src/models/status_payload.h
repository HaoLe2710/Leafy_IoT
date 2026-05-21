#pragma once

#include <Arduino.h>

namespace leafy {

struct StatusPayload {
  static String buildJson(
      const String& timestampIso,
      bool online,
      const String& ip,
      const String& wifiSsid,
      int wifiRssi,
      uint64_t uptimeSec);
};

}  // namespace leafy
