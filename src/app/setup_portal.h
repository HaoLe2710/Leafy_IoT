#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <functional>

#include "app/config_store.h"
#include "app/mqtt_manager.h"
#include "app/sensor_manager.h"
#include "app/wifi_manager.h"
#include "models/device_config.h"

namespace leafy {

class SetupPortal {
 public:
  using RuntimeStateProvider = std::function<String()>;

  SetupPortal();

  void begin(
      LocalDeviceConfig* config,
      ConfigStore* configStore,
      WifiManager* wifiManager,
      SensorManager* sensorManager,
      MqttManager* mqttManager,
      RuntimeStateProvider stateProvider);
  void start();
  void loop();
  bool isRunning() const;
  String apSsid() const;
  String portalUrl() const;

 private:
  WebServer _server;
  DNSServer _dnsServer;
  LocalDeviceConfig* _config = nullptr;
  ConfigStore* _configStore = nullptr;
  WifiManager* _wifiManager = nullptr;
  SensorManager* _sensorManager = nullptr;
  MqttManager* _mqttManager = nullptr;
  RuntimeStateProvider _stateProvider;
  bool _running = false;
  bool _dnsStarted = false;
  bool _restartScheduled = false;
  uint32_t _restartAtMs = 0;
  IPAddress _apIp;
  String _apSsid = "Leafy-Setup";
  enum : uint16_t { DNS_PORT = 53 };

  void registerRoutes();
  void startCaptiveDns();
  void scheduleRestart(uint32_t delayMs);
  String buildApSsid() const;
  String shortDeviceSuffix() const;
  String currentRuntimeState() const;
  String wifiStateText() const;
  String setupApSsid() const;
  String buildDeviceInfoJson() const;
  String htmlPage(const String& title, const String& body) const;
  String nav() const;
  String htmlEscape(const String& value) const;
  String jsonEscape(const String& value) const;
  String boolText(bool value) const;
  String formatDouble(double value, uint8_t decimals) const;
  String sensorValue(double value, bool valid, uint8_t decimals, const String& suffix = "") const;

  void handleHome();
  void handleDeviceInfo();
  void handleQrScript();
  void handleCaptivePortalProbe();
  void redirectToPortal();
  void handleWifiForm();
  void handleWifiSave();
  void handleDiagnostics();
  void handleResetConfirm();
  void handleResetPost();
  void handleApiStatus();
  void handleNotFound();
};

}  // namespace leafy
