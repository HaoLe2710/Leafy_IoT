#pragma once

#include <Arduino.h>

#include "app/mqtt_manager.h"
#include "app/wifi_manager.h"
#include "models/device_config.h"

namespace leafy {

class StatusService {
 public:
  void begin(MqttManager* mqttManager, WifiManager* wifiManager, uint32_t heartbeatIntervalSec = DEFAULT_STATUS_HEARTBEAT_SEC);
  void updateHeartbeatInterval(uint32_t heartbeatIntervalSec);
  void loop(uint32_t nowMs);
  bool publishOnlineNow();
  bool publishOfflineBestEffort();

 private:
  MqttManager* _mqttManager = nullptr;
  WifiManager* _wifiManager = nullptr;
  uint32_t _lastHeartbeatMs = 0;
  uint32_t _heartbeatIntervalMs = DEFAULT_STATUS_HEARTBEAT_SEC * 1000UL;

  bool publish(bool online);
};

}  // namespace leafy
