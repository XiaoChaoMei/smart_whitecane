#include "sensors/ultrasonic.h"

#include "config.h"

UltrasonicSensor::UltrasonicSensor(uint8_t triggerPin, uint8_t echoPin)
    : triggerPin_(triggerPin), echoPin_(echoPin) {}

void UltrasonicSensor::begin() {
  pinMode(triggerPin_, OUTPUT);
  pinMode(echoPin_, INPUT);
  digitalWrite(triggerPin_, LOW);
}

int UltrasonicSensor::readCm() const {
  digitalWrite(triggerPin_, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin_, LOW);

  const uint32_t echoUs =
      pulseIn(echoPin_, HIGH, ULTRASONIC_ECHO_TIMEOUT_US);
  if (echoUs == 0) {
    return -1;
  }

  return static_cast<int>((echoUs + 29U) / 58U);
}

UltrasonicArray::UltrasonicArray()
    : sensors_{UltrasonicSensor(FRONT_TRIG_PIN, FRONT_ECHO_PIN),
               UltrasonicSensor(LEFT_TRIG_PIN, LEFT_ECHO_PIN),
               UltrasonicSensor(RIGHT_TRIG_PIN, RIGHT_ECHO_PIN)} {}

void UltrasonicArray::begin() {
  for (auto& sensor : sensors_) {
    sensor.begin();
  }
}

void UltrasonicArray::update(uint32_t nowMs) {
  if (nowMs - lastStartMs_ < ULTRASONIC_START_INTERVAL_MS) {
    return;
  }
  lastStartMs_ = nowMs;

  const int distanceCm = sensors_[nextSensor_].readCm();
  if (nextSensor_ == 0) {
    readings_.frontCm = distanceCm;
  } else if (nextSensor_ == 1) {
    readings_.leftCm = distanceCm;
  } else {
    readings_.rightCm = distanceCm;
  }

  nextSensor_ = static_cast<uint8_t>((nextSensor_ + 1U) % 3U);
}

DistanceReadings UltrasonicArray::readings() const {
  return readings_;
}
