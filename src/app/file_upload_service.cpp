#include "app/file_upload_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include "utils/logger.h"

namespace leafy {

bool FileUploadService::isAllowedUploadEndpoint(const String& endpoint) {
  String normalized = endpoint;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized.length() == 0) {
    return false;
  }
  if (!normalized.startsWith("http://") && !normalized.startsWith("https://")) {
    return false;
  }
  return !normalized.startsWith("http://localhost") &&
         !normalized.startsWith("https://localhost") &&
         !normalized.startsWith("http://127.") &&
         !normalized.startsWith("https://127.") &&
         !normalized.startsWith("http://0.0.0.0") &&
         !normalized.startsWith("https://0.0.0.0") &&
         !normalized.startsWith("http://[::1]") &&
         !normalized.startsWith("https://[::1]");
}

bool FileUploadService::uploadMultipart(const String& endpoint, const CameraService::Frame& frame, String& fileId, String& error) {
  if (endpoint.length() == 0) {
    error = "UPLOAD_ENDPOINT_MISSING";
    Logger::warn("File upload aborted: upload endpoint is missing");
    return false;
  }
  if (!isAllowedUploadEndpoint(endpoint)) {
    error = "UPLOAD_ENDPOINT_INVALID";
    Logger::warn("File upload aborted: upload endpoint must be reachable from ESP32-CAM and cannot be localhost: " + endpoint);
    return false;
  }
  if (frame.data == nullptr || frame.size == 0) {
    error = "UPLOAD_FRAME_EMPTY";
    return false;
  }

  const String boundary = "----LeafyEsp32CamBoundary";
  const String head = "--" + boundary + "\r\n"
                      "Content-Disposition: form-data; name=\"file\"; filename=\"leafy-capture.jpg\"\r\n"
                      "Content-Type: image/jpeg\r\n\r\n";
  const String tail = "\r\n--" + boundary + "--\r\n";
  size_t bodySize = head.length() + frame.size + tail.length();
  uint8_t* body = static_cast<uint8_t*>(malloc(bodySize));
  if (body == nullptr) {
    error = "UPLOAD_BODY_ALLOC_FAILED";
    return false;
  }

  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), frame.data, frame.size);
  memcpy(body + head.length() + frame.size, tail.c_str(), tail.length());

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, endpoint)) {
    free(body);
    error = "UPLOAD_HTTP_BEGIN_FAILED";
    return false;
  }

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int status = http.POST(body, bodySize);
  String response = http.getString();
  http.end();
  free(body);

  if (status < 200 || status >= 300) {
    Logger::warn("File upload failed, status=" + String(status) + ", response=" + response);
    error = "UPLOAD_HTTP_FAILED";
    return false;
  }

  JsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, response);
  if (jsonError) {
    error = "UPLOAD_RESPONSE_INVALID_JSON";
    return false;
  }

  const char* id = doc["data"]["id"] | doc["id"] | "";
  if (id == nullptr || strlen(id) == 0) {
    error = "UPLOAD_RESPONSE_MISSING_FILE_ID";
    return false;
  }

  fileId = String(id);
  return true;
}

}  // namespace leafy
