#pragma once

#include <Arduino.h>

#include "sensors/ultrasonic.h"

class VibrationController {
 public:
  void begin();
  void update(const DistanceReadings& readings, bool waterWet,
              uint32_t nowMs);

 private:
  static uint8_t levelForDistance(int distanceCm);
  static void setMotor(uint8_t pin, uint8_t level);
};
