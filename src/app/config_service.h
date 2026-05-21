#pragma once

#include <Arduino.h>
#include <functional>

#include "app/config_store.h"
#include "app/mqtt_manager.h"
#include "models/device_config.h"

namespace leafy {

class ConfigService {
 public:
  using RuntimeApplyCallback = std::function<bool(const RuntimeConfig&, String&)>;

  void begin(LocalDeviceConfig* config, ConfigStore* configStore, MqttManager* mqttManager);
  void onApplyRuntimeConfig(RuntimeApplyCallback callback);
  bool handleConfigMessage(const String& payload);
  const String& lastApplyResult() const;

 private:
  LocalDeviceConfig* _config = nullptr;
  ConfigStore* _configStore = nullptr;
  MqttManager* _mqttManager = nullptr;
  RuntimeApplyCallback _applyRuntimeConfigCallback;
  String _lastApplyResult = "no config applied yet";

  bool parseRuntimeConfig(const String& payload, RuntimeConfig& parsed, String& errorMessage);
  bool validateRuntimeConfig(const RuntimeConfig& parsed, String& errorMessage) const;
  bool runtimeConfigEquals(const RuntimeConfig& left, const RuntimeConfig& right) const;
  bool applyRuntimeConfig(const RuntimeConfig& runtime, String& errorMessage);
  void setLastApplyResult(const String& result);
  void ack(uint32_t configVersion, bool success, const String& errorMessage = "");
};

}  // namespace leafy
