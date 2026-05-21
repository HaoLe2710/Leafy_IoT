#pragma once

#include <Arduino.h>

namespace leafy {

class BackoffTimer {
 public:
  explicit BackoffTimer(uint32_t initialDelayMs = 1000, uint32_t maxDelayMs = 30000);

  bool ready(uint32_t nowMs) const;
  void markAttempt(uint32_t nowMs);
  void reset();
  uint32_t currentDelayMs() const;

 private:
  uint32_t _initialDelayMs;
  uint32_t _maxDelayMs;
  uint32_t _currentDelayMs;
  uint32_t _lastAttemptMs = 0;
  bool _hasAttempted = false;
};

}  // namespace leafy
