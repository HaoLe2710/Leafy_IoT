#include "app/wifi_manager.h"

#include <WiFi.h>

#include "utils/logger.h"

namespace leafy {

void WifiManager::begin(const WifiConfig& config) {
  _config = config;
  if (!_config.isConfigured()) {
    enterSetupMode();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  _state = WifiState::CONNECTING;
  _backoff.reset();
  _connectAttemptInProgress = false;
  _failureCount = 0;
  connectIfReady();
}

void WifiManager::loop() {
  if (_state == WifiState::SETUP_MODE) {
    // TODO: run local portal web server when implemented.
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (_state != WifiState::CONNECTED) {
      Logger::info("Wi-Fi connected: " + ipAddress());
      _backoff.reset();
      _connectAttemptInProgress = false;
    }
    _state = WifiState::CONNECTED;
    return;
  }

  if (_state == WifiState::CONNECTED) {
    markDisconnected("connection lost");
  }

  if (_connectAttemptInProgress && millis() - _connectStartedMs > CONNECT_TIMEOUT_MS) {
    markDisconnected("connection timeout");
  }

  _state = WifiState::CONNECTING;
  connectIfReady();
}

void WifiManager::enterSetupMode(const String& apSsid) {
  Logger::warn("Entering Wi-Fi setup mode");
  _setupApSsid = apSsid.length() > 0 ? apSsid : "Leafy-Setup";
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_setupApSsid.c_str());
  _state = WifiState::SETUP_MODE;
  _connectAttemptInProgress = false;
  Logger::info("Setup AP started: SSID=" + _setupApSsid + ", IP=" + WiFi.softAPIP().toString());
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isSetupMode() const {
  return _state == WifiState::SETUP_MODE;
}

WifiState WifiManager::state() const {
  return _state;
}

String WifiManager::ipAddress() const {
  return isConnected() ? WiFi.localIP().toString() : "";
}

String WifiManager::apIpAddress() const {
  return isSetupMode() ? WiFi.softAPIP().toString() : "";
}

String WifiManager::setupApSsid() const {
  return _setupApSsid;
}

String WifiManager::getCurrentIp() const {
  return ipAddress();
}

String WifiManager::ssid() const {
  return isConnected() ? WiFi.SSID() : _config.ssid;
}

String WifiManager::getSsid() const {
  return ssid();
}

int WifiManager::rssi() const {
  return isConnected() ? WiFi.RSSI() : 0;
}

int WifiManager::getRssi() const {
  return rssi();
}

uint32_t WifiManager::failureCount() const {
  return _failureCount;
}

void WifiManager::connectIfReady() {
  uint32_t now = millis();
  if (!_backoff.ready(now)) {
    return;
  }

  Logger::info("Connecting Wi-Fi SSID=" + _config.ssid);
  WiFi.disconnect(false, false);
  WiFi.begin(_config.ssid.c_str(), _config.password.c_str());
  _connectStartedMs = now;
  _connectAttemptInProgress = true;
  _backoff.markAttempt(now);
}

void WifiManager::markDisconnected(const String& reason) {
  _failureCount++;
  _connectAttemptInProgress = false;
  WiFi.disconnect(false, false);
  Logger::warn("Wi-Fi disconnected: " + reason + ", failures=" + String(_failureCount));
}

}  // namespace leafy
