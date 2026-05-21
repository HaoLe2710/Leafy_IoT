#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "models/device_config.h"

namespace leafy {

class ConfigStore {
 public:
  bool begin();
  bool load(LocalDeviceConfig& config);
  bool hasRequiredSetup(const LocalDeviceConfig& config) const;
  bool saveWifiConfig(const WifiConfig& wifi);
  bool saveRuntimeConfig(const RuntimeConfig& runtime);
  bool saveMqttConfig(const MqttEndpointConfig& mqtt) = delete;
  bool saveIdentity(const DeviceIdentity& identity) = delete;
  bool saveCalibrationConfig(const CalibrationConfig& calibration);
  bool clearWifiConfig();
  bool clearRuntimeConfig();
  bool clearCalibrationConfig();
  bool clearRuntimeNamespace();
  bool clearAllConfig() = delete;

 private:
  Preferences _factoryPreferences;
  Preferences _runtimePreferences;
  bool _begun = false;
  bool _factoryOpened = false;

  static constexpr const char* FACTORY_NAMESPACE = "leafy_factory";
  static constexpr const char* RUNTIME_NAMESPACE = "leafy_runtime";

  RuntimeConfig defaultRuntimeConfig() const;
  CalibrationConfig defaultCalibrationConfig() const;
  String getFactoryStringOrDefault(const char* key, const String& fallback);
  uint32_t getFactoryUIntOrDefault(const char* key, uint32_t fallback);
  String getRuntimeStringOrDefault(const char* key, const String& fallback);
  uint32_t getRuntimeUIntOrDefault(const char* key, uint32_t fallback);
  bool removeRuntimeIfPresent(const char* key);
};

}  // namespace leafy
