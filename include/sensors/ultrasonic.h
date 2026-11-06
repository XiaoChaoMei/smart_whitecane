#pragma once

#include <Arduino.h>

class UltrasonicSensor {
 public:
  UltrasonicSensor(uint8_t triggerPin, uint8_t echoPin);

  void begin();
  int readCm() const;

 private:
  uint8_t triggerPin_;
  uint8_t echoPin_;
};

struct DistanceReadings {
  int frontCm = -1;
  int leftCm = -1;
  int rightCm = -1;
};

class UltrasonicArray {
 public:
  UltrasonicArray();

  void begin();
  void update(uint32_t nowMs);
  DistanceReadings readings() const;

 private:
  UltrasonicSensor sensors_[3];
  DistanceReadings readings_;
  uint8_t nextSensor_ = 0;
  uint32_t lastStartMs_ = 0;
};
