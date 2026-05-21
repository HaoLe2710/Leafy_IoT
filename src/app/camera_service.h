#pragma once

#include <Arduino.h>

namespace leafy {

class CameraService {
 public:
  struct Frame {
    uint8_t* data = nullptr;
    size_t size = 0;
    int width = 0;
    int height = 0;
    const char* contentType = "image/jpeg";
    void* nativeFrame = nullptr;
  };

  bool begin();
  bool isAvailable() const;
  bool capture(const String& resolution, const String& quality, Frame& frame, String& error);
  void release(Frame& frame);

 private:
  bool _available = false;
};

}  // namespace leafy
