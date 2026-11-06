#pragma once

#include <Arduino.h>

class Bh1750LightSensor {
 public:
  void begin();
  bool update(uint32_t nowMs);
  bool available() const;
  float lux() const;

 private:
  bool writeCommand(uint8_t command) const;

  uint8_t address_ = 0;
  float lux_ = -1.0F;
  uint32_t lastSampleMs_ = 0;
};
