#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace leafy {

class JsonUtils {
 public:
  static bool parseObject(const String& payload, JsonDocument& doc, String& errorMessage);
};

}  // namespace leafy
