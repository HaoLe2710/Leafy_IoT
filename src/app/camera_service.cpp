#include "app/camera_service.h"

#include "utils/logger.h"

#ifdef LEAFY_ESP32_CAM
#include "esp_camera.h"
#endif

namespace leafy {

bool CameraService::begin() {
#ifdef LEAFY_ESP32_CAM
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  // AI-Thinker ESP32-CAM data pins are reserved for the camera; do not map sensors to GPIO34/GPIO35.
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  _available = err == ESP_OK;
  if (_available) {
    Logger::info("ESP32-CAM camera initialized");
  } else {
    Logger::warn("ESP32-CAM camera init failed, err=" + String(err));
  }
#else
  _available = false;
#endif
  return true;
}

bool CameraService::isAvailable() const {
  return _available;
}

bool CameraService::capture(const String& resolution, const String& quality, Frame& frame, String& error) {
#ifdef LEAFY_ESP32_CAM
  if (!_available) {
    error = "CAMERA_NOT_AVAILABLE";
    Logger::warn("Camera capture refused: " + error);
    return false;
  }

  Logger::info("Camera capture requested: resolution=" + resolution + ", quality=" + quality);
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    framesize_t frameSize = resolution == "QVGA" ? FRAMESIZE_QVGA : FRAMESIZE_VGA;
    int jpegQuality = 12;
    if (quality == "LOW") {
      jpegQuality = 18;
    } else if (quality == "HIGH") {
      jpegQuality = 10;
    }
    sensor->set_framesize(sensor, frameSize);
    sensor->set_quality(sensor, jpegQuality);
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr || fb->buf == nullptr || fb->len == 0) {
    error = "CAMERA_CAPTURE_FAILED";
    Logger::warn("Camera capture failed: empty frame");
    return false;
  }

  frame.data = fb->buf;
  frame.size = fb->len;
  frame.width = fb->width;
  frame.height = fb->height;
  frame.contentType = "image/jpeg";
  frame.nativeFrame = fb;
  Logger::info("Camera capture success: bytes=" + String(frame.size) +
               ", width=" + String(frame.width) +
               ", height=" + String(frame.height));
  return true;
#else
  (void)resolution;
  (void)quality;
  (void)frame;
  error = "CAMERA_NOT_ENABLED";
  Logger::warn("Camera capture refused: " + error);
  return false;
#endif
}

void CameraService::release(Frame& frame) {
#ifdef LEAFY_ESP32_CAM
  if (frame.nativeFrame != nullptr) {
    esp_camera_fb_return(static_cast<camera_fb_t*>(frame.nativeFrame));
  }
#endif
  frame = Frame{};
}

}  // namespace leafy
