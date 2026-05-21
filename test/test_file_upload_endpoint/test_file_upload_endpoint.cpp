#include <Arduino.h>
#include <unity.h>

#include "app/file_upload_service.h"
#include "models/image_meta_payload.h"

using leafy::FileUploadService;
using leafy::buildImageMetaFailurePayload;
using leafy::buildImageMetaSuccessPayload;

namespace {

void test_lan_internal_upload_endpoint_is_allowed() {
  TEST_ASSERT_TRUE(FileUploadService::isAllowedUploadEndpoint("http://192.168.1.10:8084/internal/files/upload"));
  TEST_ASSERT_TRUE(FileUploadService::isAllowedUploadEndpoint(" https://files.example.com/internal/files/upload "));
}

void test_localhost_upload_endpoint_is_rejected() {
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint(""));
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint("http://localhost:8080/files/upload"));
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint("http://127.0.0.1:8084/internal/files/upload"));
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint("http://0.0.0.0:8084/internal/files/upload"));
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint("http://[::1]:8084/internal/files/upload"));
  TEST_ASSERT_FALSE(FileUploadService::isAllowedUploadEndpoint("ftp://192.168.1.10/files/upload"));
}

void test_image_meta_success_payload_omits_device_uid_and_uses_epoch_timestamp_fallback() {
  String payload = buildImageMetaSuccessPayload(
      "request-1",
      "MANUAL",
      "",
      "file-1",
      8857,
      640,
      480);

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, payload));
  TEST_ASSERT_FALSE(payload.indexOf("\"deviceUid\"") >= 0);
  TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z", doc["timestamp"] | "");
  TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z", doc["ts"] | "");
  TEST_ASSERT_EQUAL_STRING("SUCCESS", doc["status"] | "");
  TEST_ASSERT_TRUE(doc["success"] | false);
  TEST_ASSERT_EQUAL_STRING("file-1", doc["fileId"] | "");
  TEST_ASSERT_EQUAL_UINT32(8857, doc["sizeBytes"] | 0);
  TEST_ASSERT_EQUAL_INT(640, doc["width"] | 0);
  TEST_ASSERT_EQUAL_INT(480, doc["height"] | 0);
}

void test_image_meta_failure_payload_omits_device_uid_and_preserves_error() {
  String payload = buildImageMetaFailurePayload(
      "request-2",
      "SCHEDULED",
      "2026-05-21T04:18:52Z",
      "UPLOAD_HTTP_FAILED");

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, payload));
  TEST_ASSERT_FALSE(payload.indexOf("\"deviceUid\"") >= 0);
  TEST_ASSERT_EQUAL_STRING("2026-05-21T04:18:52Z", doc["timestamp"] | "");
  TEST_ASSERT_EQUAL_STRING("2026-05-21T04:18:52Z", doc["ts"] | "");
  TEST_ASSERT_EQUAL_STRING("FAILURE", doc["status"] | "");
  TEST_ASSERT_FALSE(doc["success"] | true);
  TEST_ASSERT_EQUAL_STRING("UPLOAD_HTTP_FAILED", doc["error"] | "");
  TEST_ASSERT_EQUAL_STRING("UPLOAD_HTTP_FAILED", doc["errorMessage"] | "");
}

}  // namespace

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_lan_internal_upload_endpoint_is_allowed);
  RUN_TEST(test_localhost_upload_endpoint_is_rejected);
  RUN_TEST(test_image_meta_success_payload_omits_device_uid_and_uses_epoch_timestamp_fallback);
  RUN_TEST(test_image_meta_failure_payload_omits_device_uid_and_preserves_error);
  UNITY_END();
}

void loop() {}
