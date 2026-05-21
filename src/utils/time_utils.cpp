#include "utils/time_utils.h"

#include <time.h>

namespace leafy {

String TimeUtils::nowIso8601() {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    return "";
  }

  struct tm timeInfo {};
  gmtime_r(&now, &timeInfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
  return String(buffer);
}

uint64_t TimeUtils::uptimeSeconds() {
  return millis() / 1000ULL;
}

}  // namespace leafy
