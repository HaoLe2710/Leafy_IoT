#include "app/setup_portal.h"

#include <ESP.h>
#include <WiFi.h>

#include "app/qr_code_script.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

#ifndef LEAFY_DEVICE_MODEL
#define LEAFY_DEVICE_MODEL "Leafy IoT Module V1"
#endif

namespace leafy {

SetupPortal::SetupPortal() : _server(80) {}

void SetupPortal::begin(
    LocalDeviceConfig* config,
    ConfigStore* configStore,
    WifiManager* wifiManager,
    SensorManager* sensorManager,
    MqttManager* mqttManager,
    RuntimeStateProvider stateProvider) {
  _config = config;
  _configStore = configStore;
  _wifiManager = wifiManager;
  _sensorManager = sensorManager;
  _mqttManager = mqttManager;
  _stateProvider = stateProvider;
  _apSsid = buildApSsid();
}

void SetupPortal::start() {
  if (_running) {
    return;
  }

  registerRoutes();
  _server.begin();
  startCaptiveDns();
  _running = true;
  Logger::info("Setup portal started: " + portalUrl());
}

void SetupPortal::loop() {
  if (!_running) {
    return;
  }

  if (_dnsStarted) {
    _dnsServer.processNextRequest();
  }
  _server.handleClient();
  if (_restartScheduled && millis() >= _restartAtMs) {
    Logger::warn("Restarting device after setup portal action");
    delay(100);
    ESP.restart();
  }
}

bool SetupPortal::isRunning() const {
  return _running;
}

String SetupPortal::apSsid() const {
  return _apSsid;
}

String SetupPortal::portalUrl() const {
  String ip = _wifiManager != nullptr ? _wifiManager->apIpAddress() : "192.168.4.1";
  if (ip.length() == 0) {
    ip = "192.168.4.1";
  }
  return "http://" + ip;
}

void SetupPortal::registerRoutes() {
  _server.on("/", HTTP_GET, [this]() { handleHome(); });
  _server.on("/device-info", HTTP_GET, [this]() { handleDeviceInfo(); });
  _server.on("/qr-code.js", HTTP_GET, [this]() { handleQrScript(); });
  _server.on("/generate_204", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/gen_204", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/library/test/success.html", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/connecttest.txt", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/fwlink", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/canonical.html", HTTP_GET, [this]() { handleCaptivePortalProbe(); });
  _server.on("/wifi", HTTP_GET, [this]() { handleWifiForm(); });
  _server.on("/wifi", HTTP_POST, [this]() { handleWifiSave(); });
  _server.on("/diagnostics", HTTP_GET, [this]() { handleDiagnostics(); });
  _server.on("/reset", HTTP_GET, [this]() { handleResetConfirm(); });
  _server.on("/reset", HTTP_POST, [this]() { handleResetPost(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
  _server.onNotFound([this]() { handleNotFound(); });
}

void SetupPortal::startCaptiveDns() {
  if (_dnsStarted) {
    return;
  }

  _apIp = WiFi.softAPIP();
  if (_apIp == IPAddress(0, 0, 0, 0)) {
    Logger::warn("Captive portal DNS not started because setup AP IP is unavailable");
    return;
  }

  _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  if (!_dnsServer.start(DNS_PORT, "*", _apIp)) {
    Logger::warn("Captive portal DNS failed to start on " + _apIp.toString() + ":" + String(DNS_PORT));
    return;
  }

  _dnsStarted = true;
  Logger::info("Captive portal DNS started on " + _apIp.toString() + ":" + String(DNS_PORT));
}

void SetupPortal::scheduleRestart(uint32_t delayMs) {
  _restartScheduled = true;
  _restartAtMs = millis() + delayMs;
}

String SetupPortal::buildApSsid() const {
  return "Leafy-Setup-" + shortDeviceSuffix();
}

String SetupPortal::shortDeviceSuffix() const {
  String source;
  if (_config != nullptr && _config->identity.deviceUid.length() > 0) {
    source = _config->identity.deviceUid;
  } else {
    source = String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  }

  source.toUpperCase();
  String suffix;
  for (uint16_t i = 0; i < source.length(); ++i) {
    char c = source.charAt(i);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      suffix += c;
    }
  }

  if (suffix.length() == 0) {
    suffix = "DEVICE";
  }
  if (suffix.length() > 6) {
    suffix = suffix.substring(suffix.length() - 6);
  }
  return suffix;
}

String SetupPortal::currentRuntimeState() const {
  if (_stateProvider) {
    return _stateProvider();
  }
  return "UNKNOWN";
}

String SetupPortal::wifiStateText() const {
  if (_wifiManager == nullptr) {
    return "unavailable";
  }

  switch (_wifiManager->state()) {
    case WifiState::DISCONNECTED:
      return "DISCONNECTED";
    case WifiState::CONNECTING:
      return "CONNECTING";
    case WifiState::CONNECTED:
      return "CONNECTED";
    case WifiState::SETUP_MODE:
      return "SETUP_MODE";
  }
  return "UNKNOWN";
}

String SetupPortal::setupApSsid() const {
  if (_wifiManager != nullptr && _wifiManager->setupApSsid().length() > 0) {
    return _wifiManager->setupApSsid();
  }
  return _apSsid;
}

String SetupPortal::buildDeviceInfoJson() const {
  String json;
  json.reserve(360);
  json += "{";
  json += "\"type\":\"LEAFY_IOT_DEVICE\",";
  json += "\"version\":1,";
  json += "\"deviceUid\":\"" + jsonEscape(_config != nullptr ? _config->identity.deviceUid : "") + "\",";
  json += "\"deviceCode\":\"" + jsonEscape(_config != nullptr ? _config->identity.deviceCode : "") + "\",";
  json += "\"deviceType\":\"" + jsonEscape(_config != nullptr ? _config->identity.deviceType : "") + "\",";
  json += "\"model\":\"" + jsonEscape(LEAFY_DEVICE_MODEL) + "\",";
  json += "\"firmwareVersion\":\"" + jsonEscape(_config != nullptr ? _config->identity.firmwareVersion : "") + "\",";
  json += "\"setupApSsid\":\"" + jsonEscape(setupApSsid()) + "\",";
  json += "\"setupPortalUrl\":\"" + jsonEscape(portalUrl()) + "\"";
  json += "}";
  return json;
}

String SetupPortal::htmlPage(const String& title, const String& body) const {
  String html;
  html.reserve(5000);
  html += "<!doctype html><html><head><meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>" + htmlEscape(title) + "</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:24px;max-width:840px;line-height:1.45;color:#1f2933}";
  html += "a{color:#0b5cab}nav a{margin-right:14px}table{border-collapse:collapse;width:100%;margin:12px 0}";
  html += "td,th{border:1px solid #d6dde5;padding:7px;text-align:left}th{background:#f3f6f9}";
  html += "input,textarea{box-sizing:border-box;width:100%;padding:9px;margin:4px 0 12px;border:1px solid #bcc7d1}";
  html += "button{padding:9px 14px;border:1px solid #1f6feb;background:#1f6feb;color:white}";
  html += ".warn{color:#9a3412}.ok{color:#166534}.muted{color:#667085}.danger{background:#b42318;border-color:#b42318}";
  html += ".card{border:1px solid #d6dde5;background:#f8fafc;padding:14px;margin:16px 0}.qr-wrap{display:flex;gap:18px;flex-wrap:wrap;align-items:flex-start}";
  html += ".qr-box{background:white;border:1px solid #d6dde5;padding:12px;min-width:220px;min-height:220px}.qr-box svg{width:220px;height:220px;display:block}";
  html += ".payload{font-family:monospace;min-height:110px;resize:vertical}.small{font-size:12px}";
  html += "</style></head><body>";
  html += "<h1>" + htmlEscape(title) + "</h1>";
  html += nav();
  html += body;
  html += "</body></html>";
  return html;
}

String SetupPortal::nav() const {
  return "<nav><a href=\"/\">Device</a><a href=\"/wifi\">Wi-Fi Setup</a><a href=\"/diagnostics\">Diagnostics</a><a href=\"/reset\">Reset</a></nav><hr>";
}

String SetupPortal::htmlEscape(const String& value) const {
  String escaped;
  escaped.reserve(value.length());
  for (uint16_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '&') {
      escaped += "&amp;";
    } else if (c == '<') {
      escaped += "&lt;";
    } else if (c == '>') {
      escaped += "&gt;";
    } else if (c == '"') {
      escaped += "&quot;";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String SetupPortal::jsonEscape(const String& value) const {
  String escaped;
  escaped.reserve(value.length());
  for (uint16_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

String SetupPortal::boolText(bool value) const {
  return value ? "true" : "false";
}

String SetupPortal::formatDouble(double value, uint8_t decimals) const {
  return String(value, static_cast<unsigned int>(decimals));
}

String SetupPortal::sensorValue(double value, bool valid, uint8_t decimals, const String& suffix) const {
  if (!valid) {
    return "unavailable";
  }
  return formatDouble(value, decimals) + suffix;
}

void SetupPortal::handleHome() {
  String body;
  String deviceInfoJson = buildDeviceInfoJson();
  body += "<p class=\"muted\">This local portal handles device-side setup and diagnostics only. Backend provision, claim code generation, and claim/bind remain in the Leafy backend/web flow.</p>";
  body += "<div class=\"card\">";
  body += "<h2>Connect this device to Leafy</h2>";
  body += "<p>Scan this QR in the Leafy app or web onboarding page.</p>";
  body += "<div class=\"qr-wrap\">";
  body += "<div id=\"device-qr\" class=\"qr-box\"><p class=\"muted small\">Rendering QR...</p></div>";
  body += "<div style=\"flex:1;min-width:260px\">";
  body += "<table><tr><th>Field</th><th>Value</th></tr>";
  body += "<tr><td>Device UID</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceUid : "") + "</td></tr>";
  body += "<tr><td>Device Code</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceCode : "") + "</td></tr>";
  body += "<tr><td>Device Type</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceType : "") + "</td></tr>";
  body += "<tr><td>Firmware</td><td>" + htmlEscape(_config != nullptr ? _config->identity.firmwareVersion : "") + "</td></tr>";
  body += "<tr><td>Setup Wi-Fi</td><td>" + htmlEscape(setupApSsid()) + "</td></tr>";
  body += "<tr><td>Portal URL</td><td>" + htmlEscape(portalUrl()) + "</td></tr>";
  body += "</table>";
  body += "</div></div>";
  body += "<label class=\"small\" for=\"device-info-payload\">QR payload JSON fallback</label>";
  body += "<textarea id=\"device-info-payload\" class=\"payload\" readonly>";
  body += htmlEscape(deviceInfoJson);
  body += "</textarea>";
  body += "<p class=\"muted small\">This payload does not include Wi-Fi passwords or MQTT broker credentials.</p>";
  body += "</div>";
  body += "<script src=\"/qr-code.js\"></script>";
  body += "<script>(function(){var payload=" + deviceInfoJson + ";var text=JSON.stringify(payload);var el=document.getElementById('device-qr');";
  body += "try{var qr=qrcode(0,'M');qr.addData(text,'Byte');qr.make();el.innerHTML=qr.createSvgTag({cellSize:5,margin:12,scalable:true,alt:'Leafy device QR'});}";
  body += "catch(e){el.innerHTML='<p class=\"warn small\">QR render failed. Use the JSON fallback below.</p>';}})();</script>";
  body += "<table><tr><th>Field</th><th>Value</th></tr>";
  body += "<tr><td>Device UID</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceUid : "") + "</td></tr>";
  body += "<tr><td>Device Code</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceCode : "") + "</td></tr>";
  body += "<tr><td>Device Type</td><td>" + htmlEscape(_config != nullptr ? _config->identity.deviceType : "") + "</td></tr>";
  body += "<tr><td>Firmware</td><td>" + htmlEscape(_config != nullptr ? _config->identity.firmwareVersion : "") + "</td></tr>";
  body += "<tr><td>Runtime State</td><td>" + htmlEscape(currentRuntimeState()) + "</td></tr>";
  body += "<tr><td>Wi-Fi State</td><td>" + htmlEscape(wifiStateText()) + "</td></tr>";
  body += "<tr><td>Setup AP SSID</td><td>" + htmlEscape(_wifiManager != nullptr ? _wifiManager->setupApSsid() : _apSsid) + "</td></tr>";
  body += "<tr><td>Portal URL</td><td>" + htmlEscape(portalUrl()) + "</td></tr>";
  body += "<tr><td>Station IP</td><td>" + htmlEscape(_wifiManager != nullptr ? _wifiManager->ipAddress() : "") + "</td></tr>";
  body += "<tr><td>MQTT State</td><td>" + String(_mqttManager != nullptr && _mqttManager->isConnected() ? "CONNECTED" : "NOT_CONNECTED") + "</td></tr>";
  body += "</table>";
  body += "<p><a href=\"/wifi\">Configure Wi-Fi</a> or open <a href=\"/diagnostics\">diagnostics</a>.</p>";
  _server.send(200, "text/html", htmlPage("Leafy Device Setup", body));
}

void SetupPortal::handleDeviceInfo() {
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "application/json", buildDeviceInfoJson());
}

void SetupPortal::handleQrScript() {
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "application/javascript", LEAFY_QR_CODE_SCRIPT);
}

void SetupPortal::handleCaptivePortalProbe() {
  redirectToPortal();
}

void SetupPortal::redirectToPortal() {
  _server.sendHeader("Location", portalUrl() + "/", true);
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(302, "text/plain", "Redirecting to Leafy setup portal...");
}

void SetupPortal::handleWifiForm() {
  String currentSsid = _config != nullptr ? _config->wifi.ssid : "";
  String body;
  body += "<p>Save local Wi-Fi credentials. The password is never displayed or logged. After saving, the device restarts and tries station mode.</p>";
  body += "<form method=\"post\" action=\"/wifi\">";
  body += "<label>SSID</label><input name=\"ssid\" maxlength=\"32\" value=\"" + htmlEscape(currentSsid) + "\" required>";
  body += "<label>Password</label><input name=\"password\" type=\"password\" maxlength=\"64\">";
  body += "<button type=\"submit\">Save Wi-Fi</button>";
  body += "</form>";
  _server.send(200, "text/html", htmlPage("Wi-Fi Setup", body));
}

void SetupPortal::handleWifiSave() {
  if (_configStore == nullptr || _config == nullptr) {
    _server.send(500, "text/html", htmlPage("Save Failed", "<p class=\"warn\">Config store is unavailable.</p>"));
    return;
  }

  String ssid = _server.arg("ssid");
  String password = _server.arg("password");
  ssid.trim();

  if (ssid.length() == 0 || ssid.length() > 32) {
    _server.send(400, "text/html", htmlPage("Save Failed", "<p class=\"warn\">SSID is required and must be 32 characters or fewer.</p>"));
    return;
  }
  if (password.length() > 64) {
    _server.send(400, "text/html", htmlPage("Save Failed", "<p class=\"warn\">Password must be 64 characters or fewer.</p>"));
    return;
  }

  WifiConfig wifi;
  wifi.ssid = ssid;
  wifi.password = password;
  if (!_configStore->saveWifiConfig(wifi)) {
    _server.send(500, "text/html", htmlPage("Save Failed", "<p class=\"warn\">Failed to save Wi-Fi config.</p>"));
    return;
  }

  _config->wifi = wifi;
  Logger::info("Wi-Fi config saved from setup portal for SSID=" + ssid);
  scheduleRestart(2500);

  String body;
  body += "<p class=\"ok\">Saved Wi-Fi credentials. The device will restart and try to connect to station Wi-Fi.</p>";
  body += "<p>If it cannot connect, power-cycle or reset Wi-Fi config to return to setup mode.</p>";
  _server.send(200, "text/html", htmlPage("Wi-Fi Saved", body));
}

void SetupPortal::handleDiagnostics() {
  SensorSnapshot snapshot;
  bool hasSensorSnapshot = false;
  if (_sensorManager != nullptr) {
    snapshot = _sensorManager->readSnapshot();
    hasSensorSnapshot = true;
  }

  String body;
  body += "<h2>Runtime</h2><table><tr><th>Field</th><th>Value</th></tr>";
  body += "<tr><td>Runtime State</td><td>" + htmlEscape(currentRuntimeState()) + "</td></tr>";
  body += "<tr><td>Wi-Fi State</td><td>" + htmlEscape(wifiStateText()) + "</td></tr>";
  body += "<tr><td>Station SSID</td><td>" + htmlEscape(_wifiManager != nullptr ? _wifiManager->ssid() : "") + "</td></tr>";
  body += "<tr><td>Station IP</td><td>" + htmlEscape(_wifiManager != nullptr ? _wifiManager->ipAddress() : "") + "</td></tr>";
  body += "<tr><td>Setup AP IP</td><td>" + htmlEscape(_wifiManager != nullptr ? _wifiManager->apIpAddress() : "") + "</td></tr>";
  body += "<tr><td>Wi-Fi RSSI</td><td>" + String(_wifiManager != nullptr && _wifiManager->isConnected() ? String(_wifiManager->rssi()) : String("unavailable")) + "</td></tr>";
  body += "<tr><td>MQTT</td><td>" + String(_mqttManager != nullptr && _mqttManager->isConnected() ? "CONNECTED" : "NOT_CONNECTED") + "</td></tr>";
  body += "<tr><td>Uptime seconds</td><td>" + String(TimeUtils::uptimeSeconds()) + "</td></tr>";
  body += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap()) + "</td></tr>";
  body += "</table>";

  body += "<h2>Effective Config</h2><table><tr><th>Field</th><th>Value</th></tr>";
  if (_config != nullptr) {
    body += "<tr><td>samplingIntervalSec</td><td>" + String(_config->runtime.samplingIntervalSec) + "</td></tr>";
    body += "<tr><td>publishIntervalSec</td><td>" + String(_config->runtime.publishIntervalSec) + "</td></tr>";
    body += "<tr><td>offlineTimeoutSec</td><td>" + String(_config->runtime.offlineTimeoutSec) + "</td></tr>";
    body += "<tr><td>alertEnabled</td><td>" + boolText(_config->runtime.alertEnabled) + "</td></tr>";
    body += "<tr><td>configVersion</td><td>" + String(_config->runtime.configVersion) + "</td></tr>";
  } else {
    body += "<tr><td colspan=\"2\">unavailable</td></tr>";
  }
  body += "</table>";

  body += "<h2>Calibration</h2><table><tr><th>Field</th><th>Value</th></tr>";
  if (_config != nullptr) {
    body += "<tr><td>soilDryRaw</td><td>" + String(_config->calibration.soilDryRaw) + "</td></tr>";
    body += "<tr><td>soilWetRaw</td><td>" + String(_config->calibration.soilWetRaw) + "</td></tr>";
    body += "<tr><td>lightDarkRaw</td><td>" + String(_config->calibration.lightDarkRaw) + "</td></tr>";
    body += "<tr><td>lightBrightRaw</td><td>" + String(_config->calibration.lightBrightRaw) + "</td></tr>";
  } else {
    body += "<tr><td colspan=\"2\">unavailable</td></tr>";
  }
  body += "</table>";

  body += "<h2>Sensor Readings</h2><table><tr><th>Metric</th><th>Value</th><th>Raw</th></tr>";
  if (hasSensorSnapshot) {
    body += "<tr><td>AIR_TEMP</td><td>" + sensorValue(snapshot.airTemp, snapshot.hasAirTemp, 1, " C") + "</td><td>n/a</td></tr>";
    body += "<tr><td>AIR_HUMIDITY</td><td>" + sensorValue(snapshot.airHumidity, snapshot.hasAirHumidity, 1, " %") + "</td><td>n/a</td></tr>";
    body += "<tr><td>SOIL_MOISTURE</td><td>" + sensorValue(snapshot.soilMoisture, snapshot.hasSoilMoisture, 1, " %") + "</td><td>" + String(snapshot.hasSoilRaw ? String(snapshot.soilRaw) : String("unavailable")) + "</td></tr>";
    body += "<tr><td>LIGHT_INTENSITY</td><td>" + sensorValue(snapshot.lightIntensity, snapshot.hasLightIntensity, 1) + "</td><td>" + String(snapshot.hasLightRaw ? String(snapshot.lightRaw) : String("unavailable")) + "</td></tr>";
  } else {
    body += "<tr><td colspan=\"3\">sensor manager unavailable</td></tr>";
  }
  body += "</table>";
  body += "<p class=\"muted\">Refresh this page to sample sensors again. Raw ADC values are for local calibration only and are not sent as backend metrics.</p>";
  _server.send(200, "text/html", htmlPage("Diagnostics", body));
}

void SetupPortal::handleResetConfirm() {
  String body;
  body += "<p class=\"warn\">This action clears runtime NVS only: Wi-Fi credentials, runtime config, topic routing overrides, sensor calibration, and schedule state. Factory identity and MQTT credentials are preserved.</p>";
  body += "<form method=\"post\" action=\"/reset\">";
  body += "<button class=\"danger\" type=\"submit\">Clear Runtime Config And Reboot</button>";
  body += "</form>";
  _server.send(200, "text/html", htmlPage("Reset Runtime Config", body));
}

void SetupPortal::handleResetPost() {
  if (_configStore == nullptr || !_configStore->clearRuntimeNamespace()) {
    _server.send(500, "text/html", htmlPage("Reset Failed", "<p class=\"warn\">Failed to clear runtime config.</p>"));
    return;
  }

  if (_config != nullptr) {
    _config->wifi.ssid = "";
    _config->wifi.password = "";
    _config->runtime = RuntimeConfig{};
    _config->calibration = CalibrationConfig{};
    _config->mqtt.productNamespace = "coffee";
    _config->mqtt.environment = "prod";
  }

  Logger::warn("Runtime NVS namespace 'leafy_runtime' cleared from setup portal; factory namespace preserved");
  scheduleRestart(2000);
  _server.send(200, "text/html", htmlPage("Runtime Config Cleared", "<p class=\"ok\">Runtime config was cleared. The device will reboot into setup mode.</p>"));
}

void SetupPortal::handleApiStatus() {
  String json;
  json.reserve(700);
  json += "{";
  json += "\"deviceUid\":\"" + jsonEscape(_config != nullptr ? _config->identity.deviceUid : "") + "\",";
  json += "\"firmwareVersion\":\"" + jsonEscape(_config != nullptr ? _config->identity.firmwareVersion : "") + "\",";
  json += "\"runtimeState\":\"" + jsonEscape(currentRuntimeState()) + "\",";
  json += "\"wifiState\":\"" + jsonEscape(wifiStateText()) + "\",";
  json += "\"setupApSsid\":\"" + jsonEscape(_wifiManager != nullptr ? _wifiManager->setupApSsid() : _apSsid) + "\",";
  json += "\"portalUrl\":\"" + jsonEscape(portalUrl()) + "\",";
  json += "\"mqttConnected\":" + boolText(_mqttManager != nullptr && _mqttManager->isConnected()) + ",";
  json += "\"uptimeSec\":" + String(TimeUtils::uptimeSeconds());
  json += "}";
  _server.send(200, "application/json", json);
}

void SetupPortal::handleNotFound() {
  if (_server.uri().startsWith("/api/")) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(404, "application/json", "{\"error\":\"not_found\"}");
    return;
  }

  redirectToPortal();
}

}  // namespace leafy
