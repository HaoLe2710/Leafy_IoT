#pragma once

#include <Arduino.h>

namespace leafy {

class TimeUtils {
 public:
  static String nowIso8601();
  static uint64_t uptimeSeconds();
};

}  // namespace leafy
