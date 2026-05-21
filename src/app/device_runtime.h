#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "app/camera_service.h"
#include "app/config_service.h"
#include "app/config_store.h"
#include "app/file_upload_service.h"
#include "app/mqtt_manager.h"
#include "app/sensor_manager.h"
#include "app/setup_portal.h"
#include "app/status_service.h"
#include "app/telemetry_service.h"
#include "app/wifi_manager.h"
#include "models/device_config.h"

namespace leafy {

enum class RuntimeState {
  BOOT,
  LOAD_LOCAL_CONFIG,
  WIFI_SETUP_MODE,
  WIFI_CONNECTING,
  MQTT_CONNECTING,
  READY,
  RUNNING,
  APPLYING_CONFIG,
  ERROR_RECOVERY
};

class DeviceRuntime {
 public:
  void begin();
  void loop();
  RuntimeState state() const;

 private:
  RuntimeState _state = RuntimeState::BOOT;
  LocalDeviceConfig _config;
  ConfigStore _configStore;
  WifiManager _wifiManager;
  MqttManager _mqttManager;
  SensorManager _sensorManager;
  TelemetryService _telemetryService;
  StatusService _statusService;
  ConfigService _configService;
  CameraService _cameraService;
  FileUploadService _fileUploadService;
  SetupPortal _setupPortal;
  bool _modulesInitialized = false;
  bool _sensorModulesInitialized = false;

  enum class CaptureState {
    IDLE,
    QUEUED,
    RETRY_WAIT
  };

  struct CaptureCommand {
    String requestId;
    String triggerType = "MANUAL";
    String resolution = "VGA";
    String quality = "MEDIUM";
    String uploadEndpoint;
    uint8_t attempts = 0;
    uint32_t nextAttemptMs = 0;
  };

  static constexpr uint8_t CAPTURE_MAX_ATTEMPTS = 3;
  static constexpr uint32_t CAPTURE_RETRY_DELAY_MS = 5000;

  CaptureState _captureState = CaptureState::IDLE;
  CaptureCommand _captureCommand;
  bool _runtimeResetButtonActive = false;
  bool _runtimeResetTriggered = false;
  bool _runtimeResetButtonLastRawPressed = false;
  uint32_t _runtimeResetButtonStartedMs = 0;
  uint32_t _runtimeResetButtonLastChangeMs = 0;

  void transitionTo(RuntimeState next);
  void enterBoot();
  void enterLoadLocalConfig();
  void enterWifiSetupMode();
  void enterWifiConnecting();
  void enterMqttConnecting();
  void enterReady();
  void runCommonLoops();
  void initializeRuntimeResetButton();
  void checkRuntimeResetButton();
  void clearRuntimeNamespaceFromButton();
  void initializeRuntimeModules();
  void initializeSensorModules();
  bool applyRuntimeConfig(const RuntimeConfig& runtime, String& errorMessage);
  void handleCameraCaptureCommand(const String& payload);
  void processPendingCapture(uint32_t now);
  void runCaptureAttempt();
  void clearCaptureCommand();
  bool isCaptureBusy() const;
  bool isScheduledCapture(const String& triggerType) const;
  String resolveUploadEndpoint(JsonDocument& doc) const;
  void publishCaptureSuccess(
      const CaptureCommand& command,
      const String& fileId,
      size_t sizeBytes,
      int width,
      int height);
  void publishCaptureFailure(const String& requestId, const String& triggerType, const String& error);
  const char* stateName(RuntimeState state) const;
};

}  // namespace leafy
