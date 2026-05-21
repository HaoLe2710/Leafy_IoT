#include <Arduino.h>

#include "app/device_runtime.h"
#include "utils/logger.h"

leafy::DeviceRuntime runtime;

void setup() {
  leafy::Logger::begin(115200);
  leafy::Logger::info("Leafy IoT firmware starting");
  runtime.begin();
}

void loop() {
  runtime.loop();
}

// #include <Arduino.h>

// void setup() {
//   Serial.begin(115200);
//   delay(2000);
//   Serial.println("BOOT OK");
// }

// void loop() {
//   Serial.println("RUNNING...");
//   delay(1000);
// }
