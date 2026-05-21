#include "app/status_service.h"

#include "models/status_payload.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

namespace leafy {

void StatusService::begin(MqttManager* mqttManager, WifiManager* wifiManager, uint32_t heartbeatIntervalSec) {
  _mqttManager = mqttManager;
  _wifiManager = wifiManager;
  updateHeartbeatInterval(heartbeatIntervalSec);
  _lastHeartbeatMs = 0;
}

void StatusService::updateHeartbeatInterval(uint32_t heartbeatIntervalSec) {
  uint32_t safeSeconds = heartbeatIntervalSec == 0 ? DEFAULT_STATUS_HEARTBEAT_SEC : heartbeatIntervalSec;
  _heartbeatIntervalMs = safeSeconds * 1000UL;
  Logger::info("Status heartbeat interval set to " + String(safeSeconds) + "s");
}

void StatusService::loop(uint32_t nowMs) {
  if (_mqttManager == nullptr || !_mqttManager->isConnected()) {
    return;
  }

  if (_lastHeartbeatMs == 0 || nowMs - _lastHeartbeatMs >= _heartbeatIntervalMs) {
    if (publish(true)) {
      _lastHeartbeatMs = nowMs;
    }
  }
}

bool StatusService::publishOnlineNow() {
  bool ok = publish(true);
  if (ok) {
    _lastHeartbeatMs = millis();
  }
  return ok;
}

bool StatusService::publishOfflineBestEffort() {
  return publish(false);
}

bool StatusService::publish(bool online) {
  if (_mqttManager == nullptr || !_mqttManager->isConnected()) {
    return false;
  }

  String ip;
  String ssid;
  int rssi = 0;
  if (_wifiManager != nullptr && _wifiManager->isConnected()) {
    ip = _wifiManager->ipAddress();
    ssid = _wifiManager->ssid();
    rssi = _wifiManager->rssi();
  }

  String payload = StatusPayload::buildJson(
      TimeUtils::nowIso8601(),
      online,
      ip,
      ssid,
      rssi,
      TimeUtils::uptimeSeconds());

  // Backend status semantics: online=true is published only when MQTT can publish.
  bool ok = _mqttManager->publishStatus(payload);
  if (!ok) {
    Logger::warn("Status publish failed");
  } else {
    Logger::info("Status published online=" + String(online ? "true" : "false"));
  }
  return ok;
}

}  // namespace leafy
