#include "app/telemetry_service.h"

#include "models/telemetry_payload.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

namespace leafy {

void TelemetryService::begin(
    const LocalDeviceConfig& config,
    SensorManager* sensorManager,
    MqttManager* mqttManager) {
  _config = config;
  _sensorManager = sensorManager;
  _mqttManager = mqttManager;
  _lastSampleMs = 0;
  _lastPublishMs = 0;
  _latestReadingsSampleMs = 0;
}

void TelemetryService::updateConfig(const RuntimeConfig& runtime) {
  _config.runtime = runtime;
  Logger::info("Telemetry intervals updated: sampleSec=" + String(runtime.samplingIntervalSec) +
               ", publishSec=" + String(runtime.publishIntervalSec));
}

void TelemetryService::updateCalibration(const CalibrationConfig& calibration) {
  _config.calibration = calibration;
  if (_sensorManager != nullptr) {
    _sensorManager->updateCalibration(calibration);
  }
}

void TelemetryService::loop(uint32_t nowMs, int wifiRssi) {
  if (sampleDue(nowMs)) {
    sample();
    _lastSampleMs = nowMs;
  }

  if (publishDue(nowMs)) {
    if (!hasPublishableReading()) {
      Logger::warn("Skipping telemetry publish because no new valid sensor readings are available");
      return;
    }

    publish(wifiRssi);
    _lastPublishMs = nowMs;
  }
}

void TelemetryService::loop(int wifiRssi) {
  loop(millis(), wifiRssi);
}

bool TelemetryService::sampleDue(uint32_t nowMs) const {
  uint32_t intervalMs = _config.runtime.samplingIntervalSec * 1000UL;
  return _lastSampleMs == 0 || nowMs - _lastSampleMs >= intervalMs;
}

bool TelemetryService::publishDue(uint32_t nowMs) const {
  uint32_t intervalMs = _config.runtime.publishIntervalSec * 1000UL;
  return _lastPublishMs == 0 || nowMs - _lastPublishMs >= intervalMs;
}

void TelemetryService::sample() {
  if (_sensorManager == nullptr) {
    return;
  }
  std::vector<SensorReading> readings = _sensorManager->readAll();
  bool valid = readingsContainValidMetric(readings);
  uint8_t validCount = validReadingCount(readings);
  if (valid) {
    _latestReadings = readings;
    _latestReadingsSampleMs = millis();
  }
  Logger::info("Telemetry sample captured: validMetrics=" + String(validCount) + "/4");
}

bool TelemetryService::hasValidReading() const {
  return readingsContainValidMetric(_latestReadings);
}

bool TelemetryService::hasPublishableReading() const {
  return hasValidReading() && _latestReadingsSampleMs > _lastPublishMs;
}

bool TelemetryService::readingsContainValidMetric(const std::vector<SensorReading>& readings) const {
  for (const SensorReading& reading : readings) {
    if (reading.valid) {
      return true;
    }
  }
  return false;
}

uint8_t TelemetryService::validReadingCount(const std::vector<SensorReading>& readings) const {
  uint8_t count = 0;
  for (const SensorReading& reading : readings) {
    if (reading.valid) {
      ++count;
    }
  }
  return count;
}

bool TelemetryService::publish(int wifiRssi) {
  if (_mqttManager == nullptr || !_mqttManager->isConnected()) {
    return false;
  }

  if (!hasPublishableReading()) {
    return false;
  }

  String payload = TelemetryPayload::buildJson(
      TimeUtils::nowIso8601(),
      _config.identity.firmwareVersion,
      -1,
      wifiRssi,
      _latestReadings);
  Logger::info("Telemetry payload=" + payload);

  if (!_mqttManager->publishTelemetry(payload)) {
    Logger::warn("Telemetry publish failed");
    return false;
  }

  Logger::info("Telemetry published: validMetrics=" + String(validReadingCount(_latestReadings)) +
               ", rssi=" + String(wifiRssi));
  return true;
}

}  // namespace leafy
