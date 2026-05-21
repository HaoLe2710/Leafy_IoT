#pragma once

#include <Arduino.h>

namespace leafy {

class Logger {
 public:
  static void begin(uint32_t baudRate = 115200);
  static void info(const String& message);
  static void warn(const String& message);
  static void error(const String& message);
  static void debug(const String& message);

 private:
  static void log(const char* level, const String& message);
};

}  // namespace leafy
