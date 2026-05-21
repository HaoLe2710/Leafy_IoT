#include "utils/retry_utils.h"

namespace leafy {

BackoffTimer::BackoffTimer(uint32_t initialDelayMs, uint32_t maxDelayMs)
    : _initialDelayMs(initialDelayMs),
      _maxDelayMs(maxDelayMs),
      _currentDelayMs(initialDelayMs) {}

bool BackoffTimer::ready(uint32_t nowMs) const {
  if (!_hasAttempted) {
    return true;
  }
  return nowMs - _lastAttemptMs >= _currentDelayMs;
}

void BackoffTimer::markAttempt(uint32_t nowMs) {
  _lastAttemptMs = nowMs;
  _hasAttempted = true;
  _currentDelayMs = min(_currentDelayMs * 2, _maxDelayMs);
}

void BackoffTimer::reset() {
  _currentDelayMs = _initialDelayMs;
  _lastAttemptMs = 0;
  _hasAttempted = false;
}

uint32_t BackoffTimer::currentDelayMs() const {
  return _currentDelayMs;
}

}  // namespace leafy
