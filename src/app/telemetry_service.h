#pragma once

#include <Arduino.h>
#include <vector>

#include "app/mqtt_manager.h"
#include "app/sensor_manager.h"
#include "models/device_config.h"

namespace leafy {

class TelemetryService {
 public:
  void begin(const LocalDeviceConfig& config, SensorManager* sensorManager, MqttManager* mqttManager);
  void updateConfig(const RuntimeConfig& runtime);
  void updateCalibration(const CalibrationConfig& calibration);
  void loop(uint32_t nowMs, int wifiRssi);
  void loop(int wifiRssi);

 private:
  LocalDeviceConfig _config;
  SensorManager* _sensorManager = nullptr;
  MqttManager* _mqttManager = nullptr;
  uint32_t _lastSampleMs = 0;
  uint32_t _lastPublishMs = 0;
  uint32_t _latestReadingsSampleMs = 0;
  std::vector<SensorReading> _latestReadings;

  bool sampleDue(uint32_t nowMs) const;
  bool publishDue(uint32_t nowMs) const;
  bool hasValidReading() const;
  bool hasPublishableReading() const;
  bool readingsContainValidMetric(const std::vector<SensorReading>& readings) const;
  uint8_t validReadingCount(const std::vector<SensorReading>& readings) const;
  void sample();
  bool publish(int wifiRssi);
};

}  // namespace leafy
