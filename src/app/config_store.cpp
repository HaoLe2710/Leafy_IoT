#include "app/config_store.h"

#include "utils/logger.h"

#ifndef LEAFY_FIRMWARE_VERSION
#define LEAFY_FIRMWARE_VERSION "leafy-esp32-dev"
#endif

#ifndef LEAFY_DEFAULT_DEVICE_UID
#define LEAFY_DEFAULT_DEVICE_UID ""
#endif

#ifndef LEAFY_DEFAULT_DEVICE_CODE
#define LEAFY_DEFAULT_DEVICE_CODE ""
#endif

#ifndef LEAFY_DEFAULT_MQTT_HOST
#define LEAFY_DEFAULT_MQTT_HOST "137.66.4.201"
#endif

#ifndef LEAFY_DEFAULT_MQTT_PORT
#define LEAFY_DEFAULT_MQTT_PORT 1883
#endif

#ifndef LEAFY_DEFAULT_MQTT_USERNAME
#define LEAFY_DEFAULT_MQTT_USERNAME "admin"
#endif

#ifndef LEAFY_DEFAULT_MQTT_PASSWORD
#define LEAFY_DEFAULT_MQTT_PASSWORD "admin"
#endif

#ifndef LEAFY_DEFAULT_WIFI_SSID
#define LEAFY_DEFAULT_WIFI_SSID ""
#endif

#ifndef LEAFY_DEFAULT_WIFI_PASSWORD
#define LEAFY_DEFAULT_WIFI_PASSWORD ""
#endif

#ifndef LEAFY_SOIL_DRY_RAW
#define LEAFY_SOIL_DRY_RAW 3200
#endif

#ifndef LEAFY_SOIL_WET_RAW
#define LEAFY_SOIL_WET_RAW 1200
#endif

#ifndef LEAFY_LIGHT_DARK_RAW
#define LEAFY_LIGHT_DARK_RAW 3500
#endif

#ifndef LEAFY_LIGHT_BRIGHT_RAW
#define LEAFY_LIGHT_BRIGHT_RAW 500
#endif

namespace leafy {

bool ConfigStore::begin() {
  if (_begun) {
    return true;
  }

  _factoryOpened = _factoryPreferences.begin(FACTORY_NAMESPACE, true);
  if (!_factoryOpened) {
    Logger::warn("NVS namespace 'leafy_factory' is unavailable; using flash-time factory defaults");
  }

  bool runtimeOpened = _runtimePreferences.begin(RUNTIME_NAMESPACE, false);
  if (!runtimeOpened) {
    if (_factoryOpened) {
      _factoryPreferences.end();
      _factoryOpened = false;
    }
    Logger::error("Failed to open NVS namespace 'leafy_runtime'");
    return false;
  }

  _begun = true;
  Logger::info("Opened NVS namespaces: factory=leafy_factory, runtime=leafy_runtime");
  return true;
}

bool ConfigStore::load(LocalDeviceConfig& config) {
  RuntimeConfig defaults = defaultRuntimeConfig();
  CalibrationConfig calibrationDefaults = defaultCalibrationConfig();

  config.identity.deviceUid = getFactoryStringOrDefault("deviceUid", LEAFY_DEFAULT_DEVICE_UID);
  config.identity.deviceCode = getFactoryStringOrDefault("deviceCode", LEAFY_DEFAULT_DEVICE_CODE);
  config.identity.deviceType = getFactoryStringOrDefault("deviceType", "ESP32");
  config.identity.firmwareVersion = LEAFY_FIRMWARE_VERSION;

  config.wifi.ssid = getRuntimeStringOrDefault("wifiSsid", LEAFY_DEFAULT_WIFI_SSID);
  config.wifi.password = getRuntimeStringOrDefault("wifiPass", LEAFY_DEFAULT_WIFI_PASSWORD);

  config.mqtt.host = getFactoryStringOrDefault("mqttHost", LEAFY_DEFAULT_MQTT_HOST);
  config.mqtt.port = static_cast<uint16_t>(getFactoryUIntOrDefault("mqttPort", LEAFY_DEFAULT_MQTT_PORT));
  config.mqtt.username = getFactoryStringOrDefault("mqttUser", LEAFY_DEFAULT_MQTT_USERNAME);
  config.mqtt.password = getFactoryStringOrDefault("mqttPass", LEAFY_DEFAULT_MQTT_PASSWORD);
  config.mqtt.productNamespace = getRuntimeStringOrDefault("mqttProduct", "coffee");
  config.mqtt.environment = getRuntimeStringOrDefault("mqttEnv", "prod");

  // Normalize legacy/local broker values to the current Fly.io production broker IP.
  if (config.mqtt.host == "192.168.1.10" ||
      config.mqtt.host == "leafy-mqtt-broker.fly.dev") {
    config.mqtt.host = LEAFY_DEFAULT_MQTT_HOST;
    config.mqtt.port = LEAFY_DEFAULT_MQTT_PORT;
    config.mqtt.username = LEAFY_DEFAULT_MQTT_USERNAME;
    config.mqtt.password = LEAFY_DEFAULT_MQTT_PASSWORD;
  }

  config.runtime.samplingIntervalSec = getRuntimeUIntOrDefault("sampleSec", defaults.samplingIntervalSec);
  config.runtime.publishIntervalSec = getRuntimeUIntOrDefault("publishSec", defaults.publishIntervalSec);
  config.runtime.offlineTimeoutSec = getRuntimeUIntOrDefault("offlineSec", defaults.offlineTimeoutSec);
  config.runtime.alertEnabled = _runtimePreferences.getBool("alertEnabled", true);
  config.runtime.configVersion = getRuntimeUIntOrDefault("cfgVersion", defaults.configVersion);
  config.calibration.soilDryRaw = static_cast<uint16_t>(getRuntimeUIntOrDefault("soilDryRaw", calibrationDefaults.soilDryRaw));
  config.calibration.soilWetRaw = static_cast<uint16_t>(getRuntimeUIntOrDefault("soilWetRaw", calibrationDefaults.soilWetRaw));
  config.calibration.lightDarkRaw = static_cast<uint16_t>(getRuntimeUIntOrDefault("lightDarkRaw", calibrationDefaults.lightDarkRaw));
  config.calibration.lightBrightRaw = static_cast<uint16_t>(getRuntimeUIntOrDefault("lightBrightRaw", calibrationDefaults.lightBrightRaw));

  Logger::info("Loaded local config: deviceUid=" + config.identity.deviceUid +
               ", wifiConfigured=" + String(config.wifi.isConfigured() ? "true" : "false") +
               ", mqtt=" + config.mqtt.host + ":" + String(config.mqtt.port) +
               ", topicPrefix=" + config.mqtt.productNamespace + "/" + config.mqtt.environment +
               ", configVersion=" + String(config.runtime.configVersion) +
               ", soilCal=" + String(config.calibration.soilDryRaw) + "/" + String(config.calibration.soilWetRaw) +
               ", lightCal=" + String(config.calibration.lightDarkRaw) + "/" + String(config.calibration.lightBrightRaw));

  return config.identity.isValid();
}

bool ConfigStore::hasRequiredSetup(const LocalDeviceConfig& config) const {
  return config.hasRequiredSetup();
}

bool ConfigStore::saveWifiConfig(const WifiConfig& wifi) {
  if (!wifi.isConfigured()) {
    Logger::warn("Refusing to save empty Wi-Fi SSID");
    return false;
  }

  bool ok = _runtimePreferences.putString("wifiSsid", wifi.ssid) > 0;
  _runtimePreferences.putString("wifiPass", wifi.password);
  Logger::info(ok ? "Saved Wi-Fi config for SSID=" + wifi.ssid : "Failed to save Wi-Fi config");
  return ok;
}

bool ConfigStore::saveRuntimeConfig(const RuntimeConfig& runtime) {
  bool wroteSample = _runtimePreferences.putUInt("sampleSec", runtime.samplingIntervalSec) > 0;
  bool wrotePublish = _runtimePreferences.putUInt("publishSec", runtime.publishIntervalSec) > 0;
  bool wroteOffline = _runtimePreferences.putUInt("offlineSec", runtime.offlineTimeoutSec) > 0;
  bool wroteAlert = _runtimePreferences.putBool("alertEnabled", runtime.alertEnabled) > 0;
  bool wroteVersion = _runtimePreferences.putUInt("cfgVersion", runtime.configVersion) > 0;

  bool verified = getRuntimeUIntOrDefault("sampleSec", 0) == runtime.samplingIntervalSec &&
                  getRuntimeUIntOrDefault("publishSec", 0) == runtime.publishIntervalSec &&
                  getRuntimeUIntOrDefault("offlineSec", 0) == runtime.offlineTimeoutSec &&
                  _runtimePreferences.getBool("alertEnabled", !runtime.alertEnabled) == runtime.alertEnabled &&
                  getRuntimeUIntOrDefault("cfgVersion", 0) == runtime.configVersion;

  bool ok = wroteSample && wrotePublish && wroteOffline && wroteAlert && wroteVersion && verified;
  Logger::info(ok ? "Saved and verified runtime config version " + String(runtime.configVersion)
                  : "Failed to save/verify runtime config");
  return ok;
}

bool ConfigStore::saveCalibrationConfig(const CalibrationConfig& calibration) {
  bool ok = true;
  ok = ok && _runtimePreferences.putUInt("soilDryRaw", calibration.soilDryRaw) > 0;
  ok = ok && _runtimePreferences.putUInt("soilWetRaw", calibration.soilWetRaw) > 0;
  ok = ok && _runtimePreferences.putUInt("lightDarkRaw", calibration.lightDarkRaw) > 0;
  ok = ok && _runtimePreferences.putUInt("lightBrightRaw", calibration.lightBrightRaw) > 0;
  Logger::info(ok ? "Saved sensor calibration config" : "Failed to save sensor calibration config");
  return ok;
}

bool ConfigStore::clearWifiConfig() {
  bool ok = true;
  ok = ok && removeRuntimeIfPresent("wifiSsid");
  ok = ok && removeRuntimeIfPresent("wifiPass");
  return ok;
}

bool ConfigStore::clearRuntimeConfig() {
  bool ok = true;
  ok = ok && removeRuntimeIfPresent("sampleSec");
  ok = ok && removeRuntimeIfPresent("publishSec");
  ok = ok && removeRuntimeIfPresent("offlineSec");
  ok = ok && removeRuntimeIfPresent("alertEnabled");
  ok = ok && removeRuntimeIfPresent("cfgVersion");
  return ok;
}

bool ConfigStore::clearCalibrationConfig() {
  bool ok = true;
  ok = ok && removeRuntimeIfPresent("soilDryRaw");
  ok = ok && removeRuntimeIfPresent("soilWetRaw");
  ok = ok && removeRuntimeIfPresent("lightDarkRaw");
  ok = ok && removeRuntimeIfPresent("lightBrightRaw");
  return ok;
}

bool ConfigStore::clearRuntimeNamespace() {
  Logger::warn("Clearing NVS namespace 'leafy_runtime'; factory namespace 'leafy_factory' is preserved");
  return _runtimePreferences.clear();
}

RuntimeConfig ConfigStore::defaultRuntimeConfig() const {
  RuntimeConfig config;
  config.samplingIntervalSec = DEFAULT_SAMPLING_INTERVAL_SEC;
  config.publishIntervalSec = DEFAULT_PUBLISH_INTERVAL_SEC;
  config.offlineTimeoutSec = DEFAULT_OFFLINE_TIMEOUT_SEC;
  config.alertEnabled = true;
  config.configVersion = DEFAULT_CONFIG_VERSION;
  return config;
}

CalibrationConfig ConfigStore::defaultCalibrationConfig() const {
  CalibrationConfig config;
  config.soilDryRaw = LEAFY_SOIL_DRY_RAW;
  config.soilWetRaw = LEAFY_SOIL_WET_RAW;
  config.lightDarkRaw = LEAFY_LIGHT_DARK_RAW;
  config.lightBrightRaw = LEAFY_LIGHT_BRIGHT_RAW;
  return config;
}

String ConfigStore::getFactoryStringOrDefault(const char* key, const String& fallback) {
  if (!_factoryOpened) {
    return fallback;
  }
  return _factoryPreferences.getString(key, fallback);
}

uint32_t ConfigStore::getFactoryUIntOrDefault(const char* key, uint32_t fallback) {
  if (!_factoryOpened) {
    return fallback;
  }
  return _factoryPreferences.getUInt(key, fallback);
}

String ConfigStore::getRuntimeStringOrDefault(const char* key, const String& fallback) {
  return _runtimePreferences.getString(key, fallback);
}

uint32_t ConfigStore::getRuntimeUIntOrDefault(const char* key, uint32_t fallback) {
  return _runtimePreferences.getUInt(key, fallback);
}

bool ConfigStore::removeRuntimeIfPresent(const char* key) {
  if (!_runtimePreferences.isKey(key)) {
    return true;
  }
  return _runtimePreferences.remove(key);
}

}  // namespace leafy
