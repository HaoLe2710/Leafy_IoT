#include <Arduino.h>
#include <unity.h>

#include "app/config_store.h"

using leafy::CalibrationConfig;
using leafy::ConfigStore;
using leafy::LocalDeviceConfig;
using leafy::RuntimeConfig;
using leafy::WifiConfig;

namespace {

ConfigStore store;

void resetRuntimeNamespace() {
  TEST_ASSERT_TRUE(store.begin());
  TEST_ASSERT_TRUE(store.clearRuntimeNamespace());
}

void test_factory_defaults_are_read_only_runtime_inputs() {
  resetRuntimeNamespace();

  LocalDeviceConfig config;
  TEST_ASSERT_TRUE(store.load(config));
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_DEVICE_UID, config.identity.deviceUid.c_str());
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_DEVICE_CODE, config.identity.deviceCode.c_str());
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_MQTT_HOST, config.mqtt.host.c_str());
  TEST_ASSERT_EQUAL_UINT16(LEAFY_DEFAULT_MQTT_PORT, config.mqtt.port);
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_MQTT_USERNAME, config.mqtt.username.c_str());
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_MQTT_PASSWORD, config.mqtt.password.c_str());
}

void test_runtime_values_are_mutable() {
  resetRuntimeNamespace();

  WifiConfig wifi;
  wifi.ssid = "LeafyTest";
  wifi.password = "test-password";
  TEST_ASSERT_TRUE(store.saveWifiConfig(wifi));

  RuntimeConfig runtime;
  runtime.samplingIntervalSec = 15;
  runtime.publishIntervalSec = 60;
  runtime.offlineTimeoutSec = 180;
  runtime.alertEnabled = false;
  runtime.configVersion = 7;
  TEST_ASSERT_TRUE(store.saveRuntimeConfig(runtime));

  CalibrationConfig calibration;
  calibration.soilDryRaw = 3000;
  calibration.soilWetRaw = 1000;
  calibration.lightDarkRaw = 3400;
  calibration.lightBrightRaw = 400;
  TEST_ASSERT_TRUE(store.saveCalibrationConfig(calibration));

  LocalDeviceConfig config;
  TEST_ASSERT_TRUE(store.load(config));
  TEST_ASSERT_EQUAL_STRING("LeafyTest", config.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_UINT32(15, config.runtime.samplingIntervalSec);
  TEST_ASSERT_EQUAL_UINT32(60, config.runtime.publishIntervalSec);
  TEST_ASSERT_EQUAL_UINT32(180, config.runtime.offlineTimeoutSec);
  TEST_ASSERT_FALSE(config.runtime.alertEnabled);
  TEST_ASSERT_EQUAL_UINT32(7, config.runtime.configVersion);
  TEST_ASSERT_EQUAL_UINT16(3000, config.calibration.soilDryRaw);
  TEST_ASSERT_EQUAL_UINT16(1000, config.calibration.soilWetRaw);
  TEST_ASSERT_EQUAL_UINT16(3400, config.calibration.lightDarkRaw);
  TEST_ASSERT_EQUAL_UINT16(400, config.calibration.lightBrightRaw);
}

void test_runtime_reset_preserves_factory_defaults() {
  resetRuntimeNamespace();

  WifiConfig wifi;
  wifi.ssid = "LeafyTest";
  wifi.password = "test-password";
  TEST_ASSERT_TRUE(store.saveWifiConfig(wifi));
  TEST_ASSERT_TRUE(store.clearRuntimeNamespace());

  LocalDeviceConfig config;
  TEST_ASSERT_TRUE(store.load(config));
  TEST_ASSERT_EQUAL_STRING("", config.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_UINT32(leafy::DEFAULT_SAMPLING_INTERVAL_SEC, config.runtime.samplingIntervalSec);
  TEST_ASSERT_EQUAL_UINT32(leafy::DEFAULT_PUBLISH_INTERVAL_SEC, config.runtime.publishIntervalSec);
  TEST_ASSERT_TRUE(config.runtime.alertEnabled);
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_DEVICE_UID, config.identity.deviceUid.c_str());
  TEST_ASSERT_EQUAL_STRING(LEAFY_DEFAULT_MQTT_HOST, config.mqtt.host.c_str());
}

}  // namespace

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_factory_defaults_are_read_only_runtime_inputs);
  RUN_TEST(test_runtime_values_are_mutable);
  RUN_TEST(test_runtime_reset_preserves_factory_defaults);
  resetRuntimeNamespace();
  UNITY_END();
}

void loop() {}
