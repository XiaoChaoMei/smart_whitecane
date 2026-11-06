#pragma once

#include <Arduino.h>

class WaterSensor {
 public:
  void begin();
  void update(uint32_t nowMs);
  bool isWet() const;
  bool consumeChanged();
  uint16_t rawValue() const;

 private:
  uint16_t rawValue_ = 0;
  bool rawWet_ = false;
  bool candidateWet_ = false;
  bool stableWet_ = false;
  bool changed_ = false;
  uint32_t candidateSinceMs_ = 0;
};
