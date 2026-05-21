#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <functional>

#include "models/device_config.h"
#include "utils/retry_utils.h"

namespace leafy {

class MqttManager {
 public:
  using ConfigMessageCallback = std::function<void(const String&)>;
  using CameraCaptureCallback = std::function<void(const String&)>;

  MqttManager();

  void begin(const LocalDeviceConfig& config);
  void loop(bool wifiConnected);
  bool isConnected();
  bool isConfigSubscribed();

  bool publishTelemetry(const String& payload);
  bool publishStatus(const String& payload);
  bool publishConfigAck(const String& payload);
  bool publishImageMeta(const String& payload);

  void onConfigMessage(ConfigMessageCallback callback);
  void onCameraCaptureMessage(CameraCaptureCallback callback);
  void handleRawMessage(char* topic, uint8_t* payload, unsigned int length);
  void simulateCameraCaptureMessage(const String& payload);
  String statusTopic() const;
  String telemetryTopic() const;
  String ackTopic() const;
  String configSetTopic() const;
  String cameraCaptureTopic() const;

 private:
  static constexpr uint16_t MQTT_BUFFER_SIZE = 1024;
  static constexpr uint32_t SUBSCRIBE_RETRY_MS = 5000;

  WiFiClient _wifiClient;
  PubSubClient _client;
  LocalDeviceConfig _config;
  BackoffTimer _backoff{1000, 30000};
  ConfigMessageCallback _configCallback;
  CameraCaptureCallback _cameraCaptureCallback;
  bool _configured = false;
  bool _configSubscribed = false;
  int _lastClientState = 0;
  uint32_t _lastSubscribeAttemptMs = 0;

  void connectIfReady();
  bool subscribeConfigTopic();
  void subscribeCameraCaptureTopic();
  String topicFor(const String& messageType) const;
  void markDisconnectedIfNeeded();
};

}  // namespace leafy
