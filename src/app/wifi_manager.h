#pragma once

#include <Arduino.h>

#include "models/device_config.h"
#include "utils/retry_utils.h"

namespace leafy {

enum class WifiState {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  SETUP_MODE
};

class WifiManager {
 public:
  void begin(const WifiConfig& config);
  void loop();
  void enterSetupMode(const String& apSsid = "Leafy-Setup");

  bool isConnected() const;
  bool isSetupMode() const;
  WifiState state() const;
  String ipAddress() const;
  String apIpAddress() const;
  String setupApSsid() const;
  String getCurrentIp() const;
  String ssid() const;
  String getSsid() const;
  int rssi() const;
  int getRssi() const;
  uint32_t failureCount() const;

 private:
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;

  WifiConfig _config;
  String _setupApSsid = "Leafy-Setup";
  WifiState _state = WifiState::DISCONNECTED;
  BackoffTimer _backoff{1000, 30000};
  uint32_t _connectStartedMs = 0;
  uint32_t _failureCount = 0;
  bool _connectAttemptInProgress = false;

  void connectIfReady();
  void markDisconnected(const String& reason);
};

}  // namespace leafy
