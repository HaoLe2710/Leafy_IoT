#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <vector>

#include "models/device_config.h"

namespace leafy {

struct SensorReading {
  String metricCode;
  double value = 0.0;
  bool valid = false;
};

struct SensorSnapshot {
  bool hasAirTemp = false;
  double airTemp = 0.0;
  bool hasAirHumidity = false;
  double airHumidity = 0.0;
  bool hasSoilMoisture = false;
  double soilMoisture = 0.0;
  bool hasSoilRaw = false;
  int soilRaw = 0;
  bool hasLightIntensity = false;
  double lightIntensity = 0.0;
  bool hasLightRaw = false;
  int lightRaw = 0;
  uint32_t sampledAtMs = 0;

  bool hasAnyValidMetric() const {
    return hasAirTemp || hasAirHumidity || hasSoilMoisture || hasLightIntensity;
  }
};

struct CalibrationRawReadings {
  bool hasSoilRaw = false;
  int soilRaw = 0;
  bool hasLightRaw = false;
  int lightRaw = 0;
  uint32_t sampledAtMs = 0;
};

class SensorManager {
 public:
  SensorManager();
  bool begin(const CalibrationConfig& calibration = CalibrationConfig{});
  SensorSnapshot readSnapshot();
  CalibrationRawReadings readCalibrationRaw();
  std::vector<SensorReading> readAll();
  void updateCalibration(const CalibrationConfig& calibration);

 private:
  CalibrationConfig _calibration;
  DHT _dht;
  Adafruit_ADS1115 _ads;
  bool _begun = false;
  bool _adsAvailable = false;
  uint8_t _adsAddress = 0;
  bool _adsReadFailureLogged = false;
  uint32_t _lastCalibrationLogMs = 0;

  bool readDht(double& temperatureC, double& humidityPercent);
  bool readSoilMoisture(double& moisturePercent, int* rawOut = nullptr);
  bool readLightIntensity(double& normalizedLight, int* rawOut = nullptr);
  int readSoilRaw();
  int readLightRaw();
  int readAdsRaw(uint8_t channel);
  int readAveragedAnalog(int pin, uint8_t samples, uint16_t sampleDelayMs);
  void maybeLogCalibrationSnapshot(const SensorSnapshot& snapshot);
  double normalizeRawToRange(int raw, int lowEndpoint, int highEndpoint, double outputMax) const;
  double clampDouble(double value, double minValue, double maxValue) const;
  double roundOneDecimal(double value) const;
  SensorReading makeReading(const String& metricCode, double value, bool valid) const;
};

}  // namespace leafy
