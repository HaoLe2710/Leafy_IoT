#include "utils/logger.h"

namespace leafy {

void Logger::begin(uint32_t baudRate) {
  Serial.begin(baudRate);
  delay(200);
}

void Logger::info(const String& message) {
  log("INFO", message);
}

void Logger::warn(const String& message) {
  log("WARN", message);
}

void Logger::error(const String& message) {
  log("ERROR", message);
}

void Logger::debug(const String& message) {
  log("DEBUG", message);
}

void Logger::log(const char* level, const String& message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] [");
  Serial.print(level);
  Serial.print("] ");
  Serial.println(message);
}

}  // namespace leafy
