#include "app/mqtt_manager.h"

#include "utils/logger.h"

namespace leafy {

namespace {
MqttManager* activeMqttManager = nullptr;

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (activeMqttManager != nullptr) {
    activeMqttManager->handleRawMessage(topic, payload, length);
  }
}
}  // namespace

MqttManager::MqttManager() : _client(_wifiClient) {}

void MqttManager::begin(const LocalDeviceConfig& config) {
  _config = config;
  activeMqttManager = this;
  _client.setServer(_config.mqtt.host.c_str(), _config.mqtt.port);
  _client.setBufferSize(MQTT_BUFFER_SIZE);
  _client.setCallback(mqttCallback);
  _configured = true;
  _configSubscribed = false;
  _lastClientState = _client.state();
  _lastSubscribeAttemptMs = 0;
}

void MqttManager::loop(bool wifiConnected) {
  if (!_configured) {
    return;
  }

  if (!wifiConnected) {
    _configSubscribed = false;
    return;
  }

  if (!_client.connected()) {
    markDisconnectedIfNeeded();
    connectIfReady();
    return;
  }

  _client.loop();
  if (!_configSubscribed) {
    uint32_t now = millis();
    if (_lastSubscribeAttemptMs == 0 || now - _lastSubscribeAttemptMs >= SUBSCRIBE_RETRY_MS) {
      _configSubscribed = subscribeConfigTopic();
    }
  }
}

bool MqttManager::isConnected() {
  return _client.connected();
}

bool MqttManager::isConfigSubscribed() {
  return _client.connected() && _configSubscribed;
}

bool MqttManager::publishTelemetry(const String& payload) {
  if (!_client.connected()) {
    return false;
  }
  return _client.publish(topicFor("telemetry").c_str(), payload.c_str());
}

bool MqttManager::publishStatus(const String& payload) {
  if (!_client.connected()) {
    return false;
  }
  return _client.publish(topicFor("status").c_str(), payload.c_str());
}

bool MqttManager::publishConfigAck(const String& payload) {
  if (!_client.connected()) {
    return false;
  }
  return _client.publish(topicFor("ack").c_str(), payload.c_str());
}

bool MqttManager::publishImageMeta(const String& payload) {
  if (!_client.connected()) {
    Logger::warn("Skipping image/meta publish because MQTT is disconnected");
    return false;
  }
  String topic = topicFor("image/meta");
  bool ok = _client.publish(topic.c_str(), payload.c_str());
  Logger::info(String(ok ? "Published" : "Failed to publish") + " image/meta topic " + topic);
  return ok;
}

void MqttManager::onConfigMessage(ConfigMessageCallback callback) {
  _configCallback = callback;
}

void MqttManager::onCameraCaptureMessage(CameraCaptureCallback callback) {
  _cameraCaptureCallback = callback;
}

void MqttManager::handleRawMessage(char* topic, uint8_t* payload, unsigned int length) {
  String topicText(topic);
  String body;
  body.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    body += static_cast<char>(payload[i]);
  }

  if (topicText == configSetTopic() && _configCallback) {
    Logger::info("Received config payload on " + topicText + ", bytes=" + String(length));
    _configCallback(body);
  } else if (topicText == cameraCaptureTopic() && _cameraCaptureCallback) {
    Logger::info("Received camera capture command on " + topicText + ", bytes=" + String(length));
    _cameraCaptureCallback(body);
  } else {
    Logger::debug("Ignoring MQTT message on topic " + topicText);
  }
}

void MqttManager::simulateCameraCaptureMessage(const String& payload) {
  if (_cameraCaptureCallback) {
    Logger::info("Simulating camera capture MQTT command, bytes=" + String(payload.length()));
    _cameraCaptureCallback(payload);
  } else {
    Logger::warn("Camera capture simulation skipped because callback is not registered");
  }
}

void MqttManager::connectIfReady() {
  uint32_t now = millis();
  if (!_backoff.ready(now)) {
    return;
  }

  String clientId = "leafy-" + _config.identity.deviceUid;
  Logger::info("Connecting MQTT broker " + _config.mqtt.host + ":" + String(_config.mqtt.port));

  bool connected;
  String willTopic = statusTopic();
  const char* willPayload = "{\"online\":false}";
  if (_config.mqtt.username.length() > 0) {
    connected = _client.connect(
        clientId.c_str(),
        _config.mqtt.username.c_str(),
        _config.mqtt.password.c_str(),
        willTopic.c_str(),
        0,
        false,
        willPayload);
  } else {
    connected = _client.connect(clientId.c_str(), willTopic.c_str(), 0, false, willPayload);
  }

  _backoff.markAttempt(now);

  if (connected) {
    Logger::info("MQTT connected");
    _backoff.reset();
    _configSubscribed = subscribeConfigTopic();
  } else {
    _lastClientState = _client.state();
    Logger::warn("MQTT connect failed, rc=" + String(_lastClientState));
  }
}

bool MqttManager::subscribeConfigTopic() {
  String topic = configSetTopic();
  _lastSubscribeAttemptMs = millis();
  if (_client.subscribe(topic.c_str())) {
    Logger::info("Subscribed config topic " + topic);
    subscribeCameraCaptureTopic();
    return true;
  }

  Logger::warn("Failed to subscribe config topic " + topic);
  return false;
}

void MqttManager::subscribeCameraCaptureTopic() {
  String topic = cameraCaptureTopic();
  if (_client.subscribe(topic.c_str())) {
    Logger::info("Subscribed camera capture topic " + topic);
  } else {
    Logger::warn("Failed to subscribe camera capture topic " + topic);
  }
}

String MqttManager::topicFor(const String& messageType) const {
  return _config.mqtt.topicPrefix() + _config.identity.deviceUid + "/" + messageType;
}

String MqttManager::statusTopic() const {
  return topicFor("status");
}

String MqttManager::telemetryTopic() const {
  return topicFor("telemetry");
}

String MqttManager::ackTopic() const {
  return topicFor("ack");
}

String MqttManager::configSetTopic() const {
  return topicFor("config/set");
}

String MqttManager::cameraCaptureTopic() const {
  return topicFor("camera/capture");
}

void MqttManager::markDisconnectedIfNeeded() {
  int currentState = _client.state();
  if (_configSubscribed || currentState != _lastClientState) {
    Logger::warn("MQTT disconnected, rc=" + String(currentState));
  }
  _configSubscribed = false;
  _lastClientState = currentState;
}

}  // namespace leafy
