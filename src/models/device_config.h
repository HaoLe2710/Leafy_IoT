#pragma once

#include <Arduino.h>

namespace leafy {

static constexpr uint32_t DEFAULT_SAMPLING_INTERVAL_SEC = 60;
static constexpr uint32_t DEFAULT_PUBLISH_INTERVAL_SEC = 300;
static constexpr uint32_t DEFAULT_OFFLINE_TIMEOUT_SEC = 900;
static constexpr uint32_t DEFAULT_CONFIG_VERSION = 1;
static constexpr uint32_t DEFAULT_STATUS_HEARTBEAT_SEC = 30;

struct CalibrationConfig {
  uint16_t soilDryRaw = 3200;
  uint16_t soilWetRaw = 1200;
  uint16_t lightDarkRaw = 3500;
  uint16_t lightBrightRaw = 500;
};

struct RuntimeConfig {
  uint32_t samplingIntervalSec = DEFAULT_SAMPLING_INTERVAL_SEC;
  uint32_t publishIntervalSec = DEFAULT_PUBLISH_INTERVAL_SEC;
  uint32_t offlineTimeoutSec = DEFAULT_OFFLINE_TIMEOUT_SEC;
  bool alertEnabled = true;
  uint32_t configVersion = DEFAULT_CONFIG_VERSION;
};

struct DeviceIdentity {
  String deviceUid;
  String deviceCode;
  String deviceType = "ESP32";
  String firmwareVersion;

  bool isValid() const {
    return deviceUid.length() > 0 && deviceCode.length() > 0;
  }
};

struct MqttEndpointConfig {
  String host = "137.66.4.201";
  uint16_t port = 1883;
  String username = "admin";
  String password = "admin";
  String productNamespace = "coffee";
  String environment = "prod";

  String topicPrefix() const {
    return productNamespace + "/" + environment + "/devices/";
  }
};

struct WifiConfig {
  String ssid;
  String password;

  bool isConfigured() const {
    return ssid.length() > 0;
  }
};

struct LocalDeviceConfig {
  DeviceIdentity identity;
  WifiConfig wifi;
  MqttEndpointConfig mqtt;
  RuntimeConfig runtime;
  CalibrationConfig calibration;

  bool hasRequiredSetup() const {
    return identity.isValid() && wifi.isConfigured() && mqtt.host.length() > 0 && mqtt.port > 0;
  }
};

}  // namespace leafy
