#include "app/device_runtime.h"

#include <ArduinoJson.h>

#include "models/image_meta_payload.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

namespace leafy {

#ifndef LEAFY_RUNTIME_RESET_BUTTON_PIN
#define LEAFY_RUNTIME_RESET_BUTTON_PIN -1
#endif

#ifndef LEAFY_RUNTIME_RESET_HOLD_MS
#define LEAFY_RUNTIME_RESET_HOLD_MS 5000
#endif

#ifndef LEAFY_RUNTIME_RESET_DEBOUNCE_MS
#define LEAFY_RUNTIME_RESET_DEBOUNCE_MS 50
#endif

void DeviceRuntime::begin() {
  Logger::info("Runtime begin");
  _state = RuntimeState::BOOT;
  initializeRuntimeResetButton();
}

void DeviceRuntime::loop() {
  checkRuntimeResetButton();

  switch (_state) {
    case RuntimeState::BOOT:
      enterBoot();
      transitionTo(RuntimeState::LOAD_LOCAL_CONFIG);
      break;

    case RuntimeState::LOAD_LOCAL_CONFIG:
      enterLoadLocalConfig();
      break;

    case RuntimeState::WIFI_SETUP_MODE:
      _wifiManager.loop();
      _setupPortal.loop();
      break;

    case RuntimeState::WIFI_CONNECTING:
      _wifiManager.loop();
      if (_wifiManager.isConnected()) {
        transitionTo(RuntimeState::MQTT_CONNECTING);
        enterMqttConnecting();
      }
      break;

    case RuntimeState::MQTT_CONNECTING:
      _wifiManager.loop();
      _mqttManager.loop(_wifiManager.isConnected());
      if (!_wifiManager.isConnected()) {
        transitionTo(RuntimeState::WIFI_CONNECTING);
      } else if (_mqttManager.isConnected() && _mqttManager.isConfigSubscribed()) {
        transitionTo(RuntimeState::READY);
      }
      break;

    case RuntimeState::READY:
      enterReady();
      break;

    case RuntimeState::RUNNING:
    {
      bool mqttWasConnected = _mqttManager.isConnected();
      runCommonLoops();
      if (!_wifiManager.isConnected()) {
        transitionTo(RuntimeState::WIFI_CONNECTING);
      } else if (!_mqttManager.isConnected()) {
        transitionTo(RuntimeState::MQTT_CONNECTING);
        enterMqttConnecting();
      } else if (!mqttWasConnected && _mqttManager.isConfigSubscribed()) {
        _statusService.publishOnlineNow();
      }
      break;
    }

    case RuntimeState::APPLYING_CONFIG:
      // Config callbacks are handled synchronously by ConfigService in v1 scaffold.
      transitionTo(RuntimeState::RUNNING);
      break;

    case RuntimeState::ERROR_RECOVERY:
      // TODO: add bounded recovery/reboot policy.
      delay(1000);
      transitionTo(RuntimeState::LOAD_LOCAL_CONFIG);
      break;
  }
}

RuntimeState DeviceRuntime::state() const {
  return _state;
}

void DeviceRuntime::transitionTo(RuntimeState next) {
  if (_state == next) {
    return;
  }
  Logger::info(String("State ") + stateName(_state) + " -> " + stateName(next));
  _state = next;
}

void DeviceRuntime::enterBoot() {
  Logger::info("Booting Leafy IoT device runtime");
}

void DeviceRuntime::enterLoadLocalConfig() {
  if (!_configStore.begin() || !_configStore.load(_config)) {
    transitionTo(RuntimeState::ERROR_RECOVERY);
    return;
  }

  if (!_configStore.hasRequiredSetup(_config)) {
    Logger::warn("Required local setup is missing; Wi-Fi setup mode required");
    transitionTo(RuntimeState::WIFI_SETUP_MODE);
    enterWifiSetupMode();
    return;
  }

  initializeRuntimeModules();
  transitionTo(RuntimeState::WIFI_CONNECTING);
  enterWifiConnecting();
}

void DeviceRuntime::initializeRuntimeModules() {
  if (_modulesInitialized) {
    return;
  }

  _configService.begin(&_config, &_configStore, &_mqttManager);
  _configService.onApplyRuntimeConfig([this](const RuntimeConfig& runtime, String& errorMessage) {
    return applyRuntimeConfig(runtime, errorMessage);
  });
  _mqttManager.onConfigMessage([this](const String& payload) {
    transitionTo(RuntimeState::APPLYING_CONFIG);
    _configService.handleConfigMessage(payload);
    transitionTo(RuntimeState::RUNNING);
  });
  _mqttManager.onCameraCaptureMessage([this](const String& payload) {
    handleCameraCaptureCommand(payload);
  });

  initializeSensorModules();
  _telemetryService.begin(_config, &_sensorManager, &_mqttManager);
  _statusService.begin(&_mqttManager, &_wifiManager, DEFAULT_STATUS_HEARTBEAT_SEC);
  _modulesInitialized = true;
}

void DeviceRuntime::enterWifiSetupMode() {
  initializeSensorModules();
  _setupPortal.begin(
      &_config,
      &_configStore,
      &_wifiManager,
      &_sensorManager,
      &_mqttManager,
      [this]() { return String(stateName(_state)); });
  _wifiManager.enterSetupMode(_setupPortal.apSsid());
  _setupPortal.start();
}

void DeviceRuntime::enterWifiConnecting() {
  _wifiManager.begin(_config.wifi);
}

void DeviceRuntime::enterMqttConnecting() {
  _mqttManager.begin(_config);
}

void DeviceRuntime::enterReady() {
  if (!_statusService.publishOnlineNow()) {
    Logger::warn("Initial online status publish failed; continuing to RUNNING and retrying in heartbeat loop");
  }
  transitionTo(RuntimeState::RUNNING);
}

void DeviceRuntime::runCommonLoops() {
  uint32_t now = millis();
  _wifiManager.loop();
  _mqttManager.loop(_wifiManager.isConnected());
  _statusService.loop(now);
  _telemetryService.loop(now, _wifiManager.rssi());
  processPendingCapture(now);
}

void DeviceRuntime::initializeRuntimeResetButton() {
#if LEAFY_RUNTIME_RESET_BUTTON_PIN >= 0
  pinMode(LEAFY_RUNTIME_RESET_BUTTON_PIN, INPUT_PULLUP);
  _runtimeResetButtonLastRawPressed = digitalRead(LEAFY_RUNTIME_RESET_BUTTON_PIN) == LOW;
  _runtimeResetButtonLastChangeMs = millis();
  Logger::info("Runtime reset button enabled on GPIO" + String(LEAFY_RUNTIME_RESET_BUTTON_PIN) +
               ", holdMs=" + String(LEAFY_RUNTIME_RESET_HOLD_MS) +
               ", debounceMs=" + String(LEAFY_RUNTIME_RESET_DEBOUNCE_MS));
#else
  Logger::info("Runtime reset button disabled; ESP32-CAM-MB EN reset is not readable by firmware");
#endif
}

void DeviceRuntime::checkRuntimeResetButton() {
#if LEAFY_RUNTIME_RESET_BUTTON_PIN >= 0
  uint32_t now = millis();
  bool rawPressed = digitalRead(LEAFY_RUNTIME_RESET_BUTTON_PIN) == LOW;

  if (rawPressed != _runtimeResetButtonLastRawPressed) {
    _runtimeResetButtonLastRawPressed = rawPressed;
    _runtimeResetButtonLastChangeMs = now;
    return;
  }

  if (now - _runtimeResetButtonLastChangeMs < LEAFY_RUNTIME_RESET_DEBOUNCE_MS) {
    return;
  }

  bool pressed = rawPressed;
  if (!pressed) {
    if (_runtimeResetButtonActive && !_runtimeResetTriggered) {
      Logger::info("Runtime reset short press ignored");
    } else if (_runtimeResetTriggered) {
      Logger::info("Runtime reset button released; long-press reset re-armed");
    }
    _runtimeResetButtonActive = false;
    _runtimeResetTriggered = false;
    _runtimeResetButtonStartedMs = 0;
    return;
  }

  if (!_runtimeResetButtonActive) {
    _runtimeResetButtonActive = true;
    _runtimeResetTriggered = false;
    _runtimeResetButtonStartedMs = now;
    Logger::warn("Runtime reset button press detected; keep holding to clear leafy_runtime");
    return;
  }

  if (_runtimeResetTriggered) {
    return;
  }

  if (now - _runtimeResetButtonStartedMs >= LEAFY_RUNTIME_RESET_HOLD_MS) {
    _runtimeResetTriggered = true;
    clearRuntimeNamespaceFromButton();
  }
#endif
}

void DeviceRuntime::clearRuntimeNamespaceFromButton() {
  Logger::warn("Runtime reset button long-press detected: clearing NVS namespace 'leafy_runtime' and preserving 'leafy_factory'");
  bool ok = _configStore.begin() && _configStore.clearRuntimeNamespace();
  if (ok) {
    Serial.println("Long press detected, runtime NVS cleared");
    Logger::warn("Runtime NVS namespace 'leafy_runtime' cleared by button long-press; device keeps running");
  } else {
    Logger::error("Runtime NVS namespace clear failed after button long-press");
  }
}

void DeviceRuntime::initializeSensorModules() {
  if (_sensorModulesInitialized) {
    return;
  }

  _sensorManager.begin(_config.calibration);
  _cameraService.begin();
  _sensorModulesInitialized = true;
}

bool DeviceRuntime::applyRuntimeConfig(const RuntimeConfig& runtime, String& errorMessage) {
  if (!_modulesInitialized) {
    errorMessage = "runtime modules are not initialized";
    return false;
  }

  _config.runtime = runtime;
  _telemetryService.updateConfig(runtime);

  // The backend config includes offlineTimeoutSec, but the current firmware
  // has no backend-defined status heartbeat field. Keep status heartbeat fixed
  // for v1 and leave offlineTimeoutSec as persisted liveness metadata.
  Logger::info("Runtime config is active: version=" + String(runtime.configVersion) +
               ", sampleSec=" + String(runtime.samplingIntervalSec) +
               ", publishSec=" + String(runtime.publishIntervalSec) +
               ", offlineSec=" + String(runtime.offlineTimeoutSec) +
               ", alertEnabled=" + String(runtime.alertEnabled ? "true" : "false") +
               ", statusHeartbeatSec=" + String(DEFAULT_STATUS_HEARTBEAT_SEC));
  return true;
}

void DeviceRuntime::handleCameraCaptureCommand(const String& payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Logger::warn("Invalid camera capture command JSON");
    publishCaptureFailure("", "UNKNOWN", "INVALID_COMMAND_JSON");
    return;
  }

  String requestId = doc["requestId"] | "";
  String triggerType = doc["triggerType"] | doc["TriggerType"] | "MANUAL";
  triggerType.toUpperCase();
  if (requestId.length() == 0) {
    publishCaptureFailure("", triggerType, "MISSING_REQUEST_ID");
    return;
  }

  if (isCaptureBusy()) {
    Logger::warn("Rejecting camera capture command because another capture is active, requestId=" + requestId);
    publishCaptureFailure(requestId, triggerType, "CAPTURE_BUSY");
    return;
  }

  String uploadMode = doc["upload"]["mode"] | "";
  String uploadEndpoint = resolveUploadEndpoint(doc);
  if (uploadMode != "FILE_SERVICE_MULTIPART") {
    Logger::warn("Invalid camera capture upload mode for requestId=" + requestId + ", mode=" + uploadMode);
    publishCaptureFailure(requestId, triggerType, "INVALID_UPLOAD_TARGET");
    return;
  }
  if (!FileUploadService::isAllowedUploadEndpoint(uploadEndpoint)) {
    Logger::warn("Invalid camera capture upload endpoint for requestId=" + requestId +
                 ". Endpoint must be provided by MQTT payload and reachable from ESP32-CAM: " + uploadEndpoint);
    publishCaptureFailure(requestId, triggerType, "INVALID_UPLOAD_TARGET");
    return;
  }

  _captureCommand = CaptureCommand{};
  _captureCommand.requestId = requestId;
  _captureCommand.triggerType = triggerType;
  _captureCommand.resolution = doc["resolution"] | "VGA";
  _captureCommand.quality = doc["quality"] | "MEDIUM";
  _captureCommand.uploadEndpoint = uploadEndpoint;
  _captureCommand.nextAttemptMs = millis();
  _captureState = CaptureState::QUEUED;

  Logger::info("Queued " + triggerType + " camera capture requestId=" + requestId);
  if (isScheduledCapture(triggerType)) {
    Logger::info("Scheduled capture command accepted for deviceUid=" + _config.identity.deviceUid);
  }
}

void DeviceRuntime::processPendingCapture(uint32_t now) {
  if (_captureState == CaptureState::IDLE) {
    return;
  }

  if (_captureState == CaptureState::RETRY_WAIT && now < _captureCommand.nextAttemptMs) {
    return;
  }

  runCaptureAttempt();
}

void DeviceRuntime::runCaptureAttempt() {
  CaptureCommand command = _captureCommand;
  _captureCommand.attempts++;

  Logger::info("Camera capture start: requestId=" + command.requestId +
               ", triggerType=" + command.triggerType +
               ", attempt=" + String(_captureCommand.attempts));

  CameraService::Frame frame;
  String error;
  if (!_cameraService.capture(command.resolution, command.quality, frame, error)) {
    if (_captureCommand.attempts < CAPTURE_MAX_ATTEMPTS) {
      _captureCommand.nextAttemptMs = millis() + CAPTURE_RETRY_DELAY_MS;
      _captureState = CaptureState::RETRY_WAIT;
      Logger::warn("Camera capture failed, retry scheduled: requestId=" + command.requestId + ", error=" + error);
    } else {
      publishCaptureFailure(command.requestId, command.triggerType, error);
      clearCaptureCommand();
    }
    return;
  }

  String fileId;
  Logger::info("Camera upload start: requestId=" + command.requestId +
               ", bytes=" + String(frame.size) +
               ", endpoint=" + command.uploadEndpoint);
  bool uploaded = _fileUploadService.uploadMultipart(command.uploadEndpoint, frame, fileId, error);
  size_t sizeBytes = frame.size;
  int width = frame.width;
  int height = frame.height;
  _cameraService.release(frame);

  if (!uploaded) {
    if (_captureCommand.attempts < CAPTURE_MAX_ATTEMPTS) {
      _captureCommand.nextAttemptMs = millis() + CAPTURE_RETRY_DELAY_MS;
      _captureState = CaptureState::RETRY_WAIT;
      Logger::warn("Camera upload failed, retry scheduled: requestId=" + command.requestId + ", error=" + error);
    } else {
      publishCaptureFailure(command.requestId, command.triggerType, error);
      clearCaptureCommand();
    }
    return;
  }

  Logger::info("Camera upload success: requestId=" + command.requestId + ", fileId=" + fileId);
  publishCaptureSuccess(command, fileId, sizeBytes, width, height);
  clearCaptureCommand();
}

void DeviceRuntime::clearCaptureCommand() {
  _captureCommand = CaptureCommand{};
  _captureState = CaptureState::IDLE;
}

bool DeviceRuntime::isCaptureBusy() const {
  return _captureState != CaptureState::IDLE;
}

bool DeviceRuntime::isScheduledCapture(const String& triggerType) const {
  return triggerType == "SCHEDULED";
}

String DeviceRuntime::resolveUploadEndpoint(JsonDocument& doc) const {
  String endpoint = doc["upload"]["endpoint"] | doc["FILE_SERVICE_UPLOAD_URL"] | "";
  endpoint.trim();
  return endpoint;
}

void DeviceRuntime::publishCaptureSuccess(
    const CaptureCommand& command,
    const String& fileId,
    size_t sizeBytes,
    int width,
    int height) {
  String resultPayload = buildImageMetaSuccessPayload(
      command.requestId,
      command.triggerType,
      TimeUtils::nowIso8601(),
      fileId,
      sizeBytes,
      width,
      height);
  if (imageMetaPayloadContainsDeviceUid(resultPayload)) {
    Logger::error("Refusing to publish image/meta success because payload contains deviceUid");
    return;
  }
  if (!_mqttManager.publishImageMeta(resultPayload)) {
    Logger::warn("Failed to publish image/meta success for requestId=" + command.requestId);
  } else {
    Logger::info("Published image/meta success for requestId=" + command.requestId);
  }
}

void DeviceRuntime::publishCaptureFailure(const String& requestId, const String& triggerType, const String& error) {
  String errorText = error.length() > 0 ? error : "CAMERA_CAPTURE_FAILED";
  String resultPayload = buildImageMetaFailurePayload(requestId, triggerType, TimeUtils::nowIso8601(), error);
  if (imageMetaPayloadContainsDeviceUid(resultPayload)) {
    Logger::error("Refusing to publish image/meta failure because payload contains deviceUid");
    return;
  }
  if (!_mqttManager.publishImageMeta(resultPayload)) {
    Logger::warn("Failed to publish image/meta failure for requestId=" + requestId);
  } else {
    Logger::info("Published image/meta failure for requestId=" + requestId + ", error=" + errorText);
  }
}

const char* DeviceRuntime::stateName(RuntimeState state) const {
  switch (state) {
    case RuntimeState::BOOT:
      return "BOOT";
    case RuntimeState::LOAD_LOCAL_CONFIG:
      return "LOAD_LOCAL_CONFIG";
    case RuntimeState::WIFI_SETUP_MODE:
      return "WIFI_SETUP_MODE";
    case RuntimeState::WIFI_CONNECTING:
      return "WIFI_CONNECTING";
    case RuntimeState::MQTT_CONNECTING:
      return "MQTT_CONNECTING";
    case RuntimeState::READY:
      return "READY";
    case RuntimeState::RUNNING:
      return "RUNNING";
    case RuntimeState::APPLYING_CONFIG:
      return "APPLYING_CONFIG";
    case RuntimeState::ERROR_RECOVERY:
      return "ERROR_RECOVERY";
  }
  return "UNKNOWN";
}

}  // namespace leafy
