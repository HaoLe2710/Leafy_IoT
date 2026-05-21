#include "utils/json_utils.h"

namespace leafy {

bool JsonUtils::parseObject(const String& payload, JsonDocument& doc, String& errorMessage) {
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    errorMessage = error.c_str();
    return false;
  }

  if (!doc.is<JsonObject>()) {
    errorMessage = "payload must be a JSON object";
    return false;
  }

  return true;
}

}  // namespace leafy
