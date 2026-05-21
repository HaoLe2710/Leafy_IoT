#include "models/image_meta_payload.h"

namespace leafy {

String normalizeImageMetaTimestamp(const String& timestamp) {
  String normalized = timestamp;
  normalized.trim();
  return normalized;
}

bool imageMetaPayloadContainsDeviceUid(const String& payload) {
  return payload.indexOf("\"deviceUid\"") >= 0;
}

String buildImageMetaSuccessPayload(
    const String& requestId,
    const String& triggerType,
    const String& timestamp,
    const String& fileId,
    size_t sizeBytes,
    int width,
    int height) {
  String normalizedTimestamp = normalizeImageMetaTimestamp(timestamp);

  JsonDocument result;
  result["timestamp"] = normalizedTimestamp;
  result["status"] = "SUCCESS";
  result["requestId"] = requestId;
  result["triggerType"] = triggerType;
  result["success"] = true;
  result["ts"] = normalizedTimestamp;
  result["fileId"] = fileId;
  result["contentType"] = "image/jpeg";
  result["sizeBytes"] = sizeBytes;
  result["width"] = width;
  result["height"] = height;
  result["error"] = nullptr;

  String payload;
  serializeJson(result, payload);
  return payload;
}

String buildImageMetaFailurePayload(
    const String& requestId,
    const String& triggerType,
    const String& timestamp,
    const String& error) {
  String normalizedTimestamp = normalizeImageMetaTimestamp(timestamp);
  String errorText = error.length() > 0 ? error : "CAMERA_CAPTURE_FAILED";

  JsonDocument result;
  result["timestamp"] = normalizedTimestamp;
  result["status"] = "FAILURE";
  result["requestId"] = requestId;
  result["triggerType"] = triggerType;
  result["success"] = false;
  result["ts"] = normalizedTimestamp;
  result["sizeBytes"] = 0;
  result["error"] = errorText;
  result["errorMessage"] = errorText;

  String payload;
  serializeJson(result, payload);
  return payload;
}

}  // namespace leafy
