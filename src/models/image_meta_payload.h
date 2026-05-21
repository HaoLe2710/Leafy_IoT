#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace leafy {

String normalizeImageMetaTimestamp(const String& timestamp);
bool imageMetaPayloadContainsDeviceUid(const String& payload);

String buildImageMetaSuccessPayload(
    const String& requestId,
    const String& triggerType,
    const String& timestamp,
    const String& fileId,
    size_t sizeBytes,
    int width,
    int height);

String buildImageMetaFailurePayload(
    const String& requestId,
    const String& triggerType,
    const String& timestamp,
    const String& error);

}  // namespace leafy
