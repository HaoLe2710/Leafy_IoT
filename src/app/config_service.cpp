#include "app/config_service.h"

#include <ArduinoJson.h>

#include "models/config_ack_payload.h"
#include "utils/json_utils.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

namespace leafy {

void ConfigService::begin(LocalDeviceConfig* config, ConfigStore* configStore, MqttManager* mqttManager) {
  _config = config;
  _configStore = configStore;
  _mqttManager = mqttManager;
}

void ConfigService::onApplyRuntimeConfig(RuntimeApplyCallback callback) {
  _applyRuntimeConfigCallback = callback;
}

bool ConfigService::handleConfigMessage(const String& payload) {
  if (_config == nullptr || _configStore == nullptr) {
    setLastApplyResult("config service not initialized");
    return false;
  }

  RuntimeConfig parsed;
  String errorMessage;
  Logger::info("Config payload received, bytes=" + String(payload.length()));
  if (!parseRuntimeConfig(payload, parsed, errorMessage)) {
    Logger::warn("Config parse failed: " + errorMessage);
    setLastApplyResult("rejected: " + errorMessage);
    ack(_config->runtime.configVersion, false, errorMessage);
    return false;
  }

  if (!validateRuntimeConfig(parsed, errorMessage)) {
    Logger::warn("Config validation failed: " + errorMessage);
    setLastApplyResult("rejected version " + String(parsed.configVersion) + ": " + errorMessage);
    ack(parsed.configVersion, false, errorMessage);
    return false;
  }

  RuntimeConfig previous = _config->runtime;
  if (parsed.configVersion < _config->runtime.configVersion) {
    Logger::warn("Ignoring stale config version " + String(parsed.configVersion) +
                 ", local=" + String(_config->runtime.configVersion));
    setLastApplyResult("ignored stale version " + String(parsed.configVersion));
    ack(parsed.configVersion, false, "stale config version");
    return false;
  }

  // Same-version payloads are idempotent only when every runtime field matches.
  // A same version with different values is rejected so the device does not
  // silently accept conflicting config under a non-incremented version.
  if (parsed.configVersion == previous.configVersion && !runtimeConfigEquals(parsed, previous)) {
    errorMessage = "same configVersion with different values";
    Logger::warn("Config validation failed: " + errorMessage);
    setLastApplyResult("rejected version " + String(parsed.configVersion) + ": " + errorMessage);
    ack(parsed.configVersion, false, errorMessage);
    return false;
  }

  if (!applyRuntimeConfig(parsed, errorMessage)) {
    Logger::warn("Config apply failed: " + errorMessage);
    setLastApplyResult("apply failed version " + String(parsed.configVersion) + ": " + errorMessage);
    ack(parsed.configVersion, false, errorMessage);
    return false;
  }

  if (parsed.configVersion == previous.configVersion) {
    Logger::info("Config version " + String(parsed.configVersion) + " re-applied idempotently");
    setLastApplyResult("applied idempotent version " + String(parsed.configVersion));
    ack(parsed.configVersion, true);
    return true;
  }

  if (!_configStore->saveRuntimeConfig(parsed)) {
    if (!_configStore->saveRuntimeConfig(previous)) {
      Logger::error("Failed to restore previous runtime config in NVS after persistence failure");
    }
    String rollbackError;
    if (!applyRuntimeConfig(previous, rollbackError)) {
      Logger::error("Runtime config rollback failed after persistence failure: " + rollbackError);
    }
    setLastApplyResult("persist failed version " + String(parsed.configVersion));
    ack(parsed.configVersion, false, "failed to persist config");
    return false;
  }

  Logger::info("Applied and stored runtime config version " + String(parsed.configVersion) +
               ", sampleSec=" + String(parsed.samplingIntervalSec) +
               ", publishSec=" + String(parsed.publishIntervalSec) +
               ", offlineSec=" + String(parsed.offlineTimeoutSec) +
               ", alertEnabled=" + String(parsed.alertEnabled ? "true" : "false"));
  setLastApplyResult("applied version " + String(parsed.configVersion));
  ack(parsed.configVersion, true);
  return true;
}

const String& ConfigService::lastApplyResult() const {
  return _lastApplyResult;
}

bool ConfigService::parseRuntimeConfig(const String& payload, RuntimeConfig& parsed, String& errorMessage) {
  JsonDocument doc;
  if (!JsonUtils::parseObject(payload, doc, errorMessage)) {
    return false;
  }

  if (!doc["samplingIntervalSec"].is<uint32_t>() ||
      !doc["publishIntervalSec"].is<uint32_t>() ||
      !doc["offlineTimeoutSec"].is<uint32_t>() ||
      !doc["alertEnabled"].is<bool>() ||
      !doc["configVersion"].is<uint32_t>()) {
    errorMessage = "missing or invalid config fields";
    return false;
  }

  parsed.samplingIntervalSec = doc["samplingIntervalSec"].as<uint32_t>();
  parsed.publishIntervalSec = doc["publishIntervalSec"].as<uint32_t>();
  parsed.offlineTimeoutSec = doc["offlineTimeoutSec"].as<uint32_t>();
  parsed.alertEnabled = doc["alertEnabled"].as<bool>();
  parsed.configVersion = doc["configVersion"].as<uint32_t>();
  return true;
}

bool ConfigService::validateRuntimeConfig(const RuntimeConfig& parsed, String& errorMessage) const {
  if (parsed.configVersion < 1) {
    errorMessage = "configVersion must be >= 1";
    return false;
  }
  if (parsed.samplingIntervalSec == 0 ||
      parsed.publishIntervalSec == 0 ||
      parsed.offlineTimeoutSec == 0) {
    errorMessage = "intervals must be positive";
    return false;
  }
  if (parsed.publishIntervalSec < parsed.samplingIntervalSec) {
    errorMessage = "publishIntervalSec must be >= samplingIntervalSec";
    return false;
  }
  if (parsed.offlineTimeoutSec <= parsed.publishIntervalSec) {
    errorMessage = "offlineTimeoutSec must be > publishIntervalSec";
    return false;
  }
  return true;
}

bool ConfigService::runtimeConfigEquals(const RuntimeConfig& left, const RuntimeConfig& right) const {
  return left.samplingIntervalSec == right.samplingIntervalSec &&
         left.publishIntervalSec == right.publishIntervalSec &&
         left.offlineTimeoutSec == right.offlineTimeoutSec &&
         left.alertEnabled == right.alertEnabled &&
         left.configVersion == right.configVersion;
}

bool ConfigService::applyRuntimeConfig(const RuntimeConfig& runtime, String& errorMessage) {
  if (_applyRuntimeConfigCallback) {
    return _applyRuntimeConfigCallback(runtime, errorMessage);
  }

  if (_config == nullptr) {
    errorMessage = "config pointer unavailable";
    return false;
  }

  _config->runtime = runtime;
  return true;
}

void ConfigService::setLastApplyResult(const String& result) {
  _lastApplyResult = result;
  Logger::info("Config apply result: " + _lastApplyResult);
}

void ConfigService::ack(uint32_t configVersion, bool success, const String& errorMessage) {
  if (_mqttManager == nullptr || !_mqttManager->isConnected()) {
    Logger::warn("Config ACK skipped because MQTT is not connected");
    return;
  }

  String payload = ConfigAckPayload::buildJson(
      TimeUtils::nowIso8601(),
      configVersion,
      success,
      errorMessage);

  if (!_mqttManager->publishConfigAck(payload)) {
    Logger::warn("Config ACK publish failed");
    return;
  }

  Logger::info("Config ACK published: version=" + String(configVersion) +
               ", success=" + String(success ? "true" : "false"));
}

}  // namespace leafy
