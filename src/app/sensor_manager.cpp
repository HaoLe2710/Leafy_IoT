#include "app/sensor_manager.h"

#include <Wire.h>
#include <math.h>

#include "utils/logger.h"

#ifndef LEAFY_DHT_PIN
#define LEAFY_DHT_PIN 13
#endif

#ifndef LEAFY_I2C_SDA_PIN
#define LEAFY_I2C_SDA_PIN 15
#endif

#ifndef LEAFY_I2C_SCL_PIN
#define LEAFY_I2C_SCL_PIN 14
#endif

#ifndef LEAFY_USE_ADS1115
#define LEAFY_USE_ADS1115 0
#endif

#ifndef LEAFY_SOIL_ADS_CHANNEL
#define LEAFY_SOIL_ADS_CHANNEL 1
#endif

#ifndef LEAFY_LDR_ADS_CHANNEL
#define LEAFY_LDR_ADS_CHANNEL 0
#endif

#ifndef LEAFY_SOIL_ADC_PIN
#define LEAFY_SOIL_ADC_PIN -1
#endif

#ifndef LEAFY_SOIL_POWER_PIN
#define LEAFY_SOIL_POWER_PIN -1
#endif

#ifndef LEAFY_LDR_ADC_PIN
#define LEAFY_LDR_ADC_PIN -1
#endif

#ifndef LEAFY_ANALOG_MAX
#define LEAFY_ANALOG_MAX 4095
#endif

#ifndef LEAFY_CALIBRATION_LOGGING
#define LEAFY_CALIBRATION_LOGGING 0
#endif

#ifndef LEAFY_CALIBRATION_LOG_INTERVAL_SEC
#define LEAFY_CALIBRATION_LOG_INTERVAL_SEC 30
#endif

namespace leafy {

SensorManager::SensorManager() : _dht(LEAFY_DHT_PIN, DHT11) {}

bool SensorManager::begin(const CalibrationConfig& calibration) {
  _calibration = calibration;
  Logger::info("Initializing sensors: DHT11 pin=" + String(LEAFY_DHT_PIN) +
               ", i2cSdaPin=" + String(LEAFY_I2C_SDA_PIN) +
               ", i2cSclPin=" + String(LEAFY_I2C_SCL_PIN) +
               ", ads1115=" + String(LEAFY_USE_ADS1115 ? "enabled" : "disabled") +
               ", soilAdcPin=" + String(LEAFY_SOIL_ADC_PIN) +
               ", soilAdsChannel=" + String(LEAFY_SOIL_ADS_CHANNEL) +
               ", soilPowerPin=" + String(LEAFY_SOIL_POWER_PIN) +
               ", ldrAdcPin=" + String(LEAFY_LDR_ADC_PIN) +
               ", ldrAdsChannel=" + String(LEAFY_LDR_ADS_CHANNEL));
  Logger::info("Sensor calibration active: soilDryRaw=" + String(_calibration.soilDryRaw) +
               ", soilWetRaw=" + String(_calibration.soilWetRaw) +
               ", lightDarkRaw=" + String(_calibration.lightDarkRaw) +
               ", lightBrightRaw=" + String(_calibration.lightBrightRaw));

  _dht.begin();

  // The PCB schematic routes ADS1115 SDA to GPIO15 and SCL to GPIO14.
  Wire.begin(LEAFY_I2C_SDA_PIN, LEAFY_I2C_SCL_PIN);
  Wire.setClock(100000);
#if LEAFY_USE_ADS1115
  _adsAvailable = false;
  _adsAddress = 0;
  for (uint8_t address = 0x48; address <= 0x4B; ++address) {
    if (_ads.begin(address, &Wire)) {
      _adsAvailable = true;
      _adsAddress = address;
      break;
    }
  }
  if (_adsAvailable) {
    _ads.setGain(GAIN_ONE);
    Logger::info("ADS1115 initialized at address 0x" + String(_adsAddress, HEX) +
                 " on SDA=" + String(LEAFY_I2C_SDA_PIN) +
                 ", SCL=" + String(LEAFY_I2C_SCL_PIN));
  } else {
    Logger::warn("ADS1115 not detected on configured I2C pins; analog ADS readings will be invalid");
  }
#else
  _adsAvailable = false;
#endif

#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
#if LEAFY_SOIL_ADC_PIN >= 0
  analogSetPinAttenuation(LEAFY_SOIL_ADC_PIN, ADC_11db);
#endif
#if LEAFY_LDR_ADC_PIN >= 0
  analogSetPinAttenuation(LEAFY_LDR_ADC_PIN, ADC_11db);
#endif
#endif

#if LEAFY_SOIL_ADC_PIN >= 0
  pinMode(LEAFY_SOIL_ADC_PIN, INPUT);
#endif
#if LEAFY_LDR_ADC_PIN >= 0
  pinMode(LEAFY_LDR_ADC_PIN, INPUT);
#endif

#if LEAFY_SOIL_POWER_PIN >= 0
  pinMode(LEAFY_SOIL_POWER_PIN, OUTPUT);
  digitalWrite(LEAFY_SOIL_POWER_PIN, LOW);
#else
  Logger::warn("YL-69 power control pin is disabled; continuous sensor power increases corrosion risk");
#endif

  _begun = true;
  return true;
}

void SensorManager::updateCalibration(const CalibrationConfig& calibration) {
  _calibration = calibration;
  Logger::info("Sensor calibration active: soilDryRaw=" + String(_calibration.soilDryRaw) +
               ", soilWetRaw=" + String(_calibration.soilWetRaw) +
               ", lightDarkRaw=" + String(_calibration.lightDarkRaw) +
               ", lightBrightRaw=" + String(_calibration.lightBrightRaw));
}

SensorSnapshot SensorManager::readSnapshot() {
  if (!_begun) {
    Logger::warn("SensorManager read requested before begin()");
  }

  SensorSnapshot snapshot;
  snapshot.sampledAtMs = millis();

  double temperatureC = 0.0;
  double humidityPercent = 0.0;
  if (readDht(temperatureC, humidityPercent)) {
    snapshot.hasAirTemp = true;
    snapshot.airTemp = roundOneDecimal(temperatureC);
    snapshot.hasAirHumidity = true;
    snapshot.airHumidity = roundOneDecimal(humidityPercent);
  }

  double soilMoisture = 0.0;
  int soilRaw = 0;
  if (readSoilMoisture(soilMoisture, &soilRaw)) {
    snapshot.hasSoilMoisture = true;
    snapshot.soilMoisture = roundOneDecimal(soilMoisture);
    snapshot.hasSoilRaw = true;
    snapshot.soilRaw = soilRaw;
  }

  double lightIntensity = 0.0;
  int lightRaw = 0;
  if (readLightIntensity(lightIntensity, &lightRaw)) {
    snapshot.hasLightIntensity = true;
    snapshot.lightIntensity = roundOneDecimal(lightIntensity);
    snapshot.hasLightRaw = true;
    snapshot.lightRaw = lightRaw;
  }

  maybeLogCalibrationSnapshot(snapshot);
  return snapshot;
}

CalibrationRawReadings SensorManager::readCalibrationRaw() {
  CalibrationRawReadings readings;
  readings.sampledAtMs = millis();

  int soilRaw = readSoilRaw();
  if (soilRaw >= 0) {
    readings.hasSoilRaw = true;
    readings.soilRaw = soilRaw;
  }

  int lightRaw = readLightRaw();
  if (lightRaw >= 0) {
    readings.hasLightRaw = true;
    readings.lightRaw = lightRaw;
  }

  return readings;
}

std::vector<SensorReading> SensorManager::readAll() {
  SensorSnapshot snapshot = readSnapshot();

  std::vector<SensorReading> readings;
  readings.reserve(4);
  readings.push_back(makeReading("AIR_TEMP", snapshot.airTemp, snapshot.hasAirTemp));
  readings.push_back(makeReading("AIR_HUMIDITY", snapshot.airHumidity, snapshot.hasAirHumidity));
  readings.push_back(makeReading("SOIL_MOISTURE", snapshot.soilMoisture, snapshot.hasSoilMoisture));
  readings.push_back(makeReading("LIGHT_INTENSITY", snapshot.lightIntensity, snapshot.hasLightIntensity));
  return readings;
}

bool SensorManager::readDht(double& temperatureC, double& humidityPercent) {
  float humidity = _dht.readHumidity();
  float temperature = _dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Logger::warn("DHT11 read failed; omitting AIR_TEMP and AIR_HUMIDITY for this sample");
    return false;
  }

  if (humidity < 0.0f || humidity > 100.0f || temperature < -20.0f || temperature > 80.0f) {
    Logger::warn("DHT11 read out of expected range; temp=" + String(temperature) +
                 ", humidity=" + String(humidity));
    return false;
  }

  temperatureC = temperature;
  humidityPercent = humidity;
  return true;
}

bool SensorManager::readSoilMoisture(double& moisturePercent, int* rawOut) {
  int raw = readSoilRaw();
  if (rawOut != nullptr) {
    *rawOut = raw;
  }

  if (raw < 0) {
    Logger::warn("YL-69 soil moisture analog read failed");
    return false;
  }

  moisturePercent = normalizeRawToRange(raw, _calibration.soilDryRaw, _calibration.soilWetRaw, 100.0);
  Logger::debug("Soil raw=" + String(raw) + ", moisture=" + String(moisturePercent));
  return true;
}

bool SensorManager::readLightIntensity(double& normalizedLight, int* rawOut) {
  int raw = readLightRaw();
  if (rawOut != nullptr) {
    *rawOut = raw;
  }

  if (raw < 0) {
    Logger::warn("LDR analog read failed");
    return false;
  }

  // This is a normalized 0..1000 brightness scale, not calibrated lux.
  normalizedLight = normalizeRawToRange(raw, _calibration.lightDarkRaw, _calibration.lightBrightRaw, 1000.0);
  Logger::debug("Light raw=" + String(raw) + ", normalized=" + String(normalizedLight));
  return true;
}

int SensorManager::readSoilRaw() {
#if LEAFY_SOIL_POWER_PIN >= 0
  digitalWrite(LEAFY_SOIL_POWER_PIN, HIGH);
  delay(100);
#endif

#if LEAFY_USE_ADS1115
  int raw = readAdsRaw(LEAFY_SOIL_ADS_CHANNEL);
#else
  int raw = readAveragedAnalog(LEAFY_SOIL_ADC_PIN, 8, 8);
#endif

#if LEAFY_SOIL_POWER_PIN >= 0
  digitalWrite(LEAFY_SOIL_POWER_PIN, LOW);
#endif

  return raw;
}

int SensorManager::readLightRaw() {
#if LEAFY_USE_ADS1115
  return readAdsRaw(LEAFY_LDR_ADS_CHANNEL);
#else
  return readAveragedAnalog(LEAFY_LDR_ADC_PIN, 8, 4);
#endif
}

int SensorManager::readAdsRaw(uint8_t channel) {
  if (!_adsAvailable) {
    if (!_adsReadFailureLogged) {
      Logger::warn("ADS1115 read failed because no ADS1115 was detected on I2C SDA=" +
                   String(LEAFY_I2C_SDA_PIN) + ", SCL=" + String(LEAFY_I2C_SCL_PIN));
      _adsReadFailureLogged = true;
    }
    return -1;
  }

  if (channel > 3) {
    Logger::warn("ADS1115 read failed because channel is out of range: " + String(channel));
    return -1;
  }

  int16_t raw = _ads.readADC_SingleEnded(channel);
  if (raw < 0) {
    if (!_adsReadFailureLogged) {
      Logger::warn("ADS1115 channel " + String(channel) + " returned negative raw value " + String(raw) +
                   "; disabling ADS1115 reads until next reboot");
      _adsReadFailureLogged = true;
    }
    _adsAvailable = false;
    return -1;
  }

  // Keep existing 0..4095 calibration constants usable while sourcing analog data from ADS1115.
  return static_cast<int>((static_cast<int32_t>(raw) * LEAFY_ANALOG_MAX) / 32767);
}

int SensorManager::readAveragedAnalog(int pin, uint8_t samples, uint16_t sampleDelayMs) {
  if (pin < 0 || samples == 0) {
    return -1;
  }

  uint32_t total = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    total += analogRead(pin);
    if (sampleDelayMs > 0) {
      delay(sampleDelayMs);
    }
  }

  return static_cast<int>(total / samples);
}

void SensorManager::maybeLogCalibrationSnapshot(const SensorSnapshot& snapshot) {
#if LEAFY_CALIBRATION_LOGGING
  uint32_t now = millis();
  uint32_t intervalMs = LEAFY_CALIBRATION_LOG_INTERVAL_SEC * 1000UL;
  if (_lastCalibrationLogMs != 0 && now - _lastCalibrationLogMs < intervalMs) {
    return;
  }

  _lastCalibrationLogMs = now;
  Logger::info("Calibration sample: soilRaw=" +
               String(snapshot.hasSoilRaw ? String(snapshot.soilRaw) : String("invalid")) +
               ", soilPct=" +
               String(snapshot.hasSoilMoisture ? String(snapshot.soilMoisture, 1) : String("invalid")) +
               ", lightRaw=" +
               String(snapshot.hasLightRaw ? String(snapshot.lightRaw) : String("invalid")) +
               ", lightNorm=" +
               String(snapshot.hasLightIntensity ? String(snapshot.lightIntensity, 1) : String("invalid")) +
               ", soilCal=" + String(_calibration.soilDryRaw) + "/" + String(_calibration.soilWetRaw) +
               ", lightCal=" + String(_calibration.lightDarkRaw) + "/" + String(_calibration.lightBrightRaw));
#else
  (void)snapshot;
#endif
}

double SensorManager::normalizeRawToRange(int raw, int lowEndpoint, int highEndpoint, double outputMax) const {
  if (lowEndpoint == highEndpoint) {
    return 0.0;
  }

  double normalized = (static_cast<double>(raw) - static_cast<double>(lowEndpoint)) /
                      (static_cast<double>(highEndpoint) - static_cast<double>(lowEndpoint));
  return clampDouble(normalized * outputMax, 0.0, outputMax);
}

double SensorManager::clampDouble(double value, double minValue, double maxValue) const {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

double SensorManager::roundOneDecimal(double value) const {
  return round(value * 10.0) / 10.0;
}

SensorReading SensorManager::makeReading(const String& metricCode, double value, bool valid) const {
  SensorReading reading;
  reading.metricCode = metricCode;
  reading.value = value;
  reading.valid = valid;
  return reading;
}

}  // namespace leafy
