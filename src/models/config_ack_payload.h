#pragma once

#include <Arduino.h>

namespace leafy {

struct ConfigAckPayload {
  static String buildJson(
      const String& timestampIso,
      uint32_t configVersion,
      bool success,
      const String& errorMessage = "");
};

}  // namespace leafy
