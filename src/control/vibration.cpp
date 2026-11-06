#include "control/vibration.h"

#include <algorithm>

#include "config.h"

void VibrationController::begin() {
  ledcAttach(LEFT_MOTOR_PIN, 1000, 8);
  ledcAttach(RIGHT_MOTOR_PIN, 1000, 8);
  setMotor(LEFT_MOTOR_PIN, 0);
  setMotor(RIGHT_MOTOR_PIN, 0);
}

void VibrationController::update(const DistanceReadings& readings,
                                 bool waterWet, uint32_t nowMs) {
  uint8_t leftLevel = levelForDistance(readings.leftCm);
  uint8_t rightLevel = levelForDistance(readings.rightCm);
  const uint8_t frontLevel = levelForDistance(readings.frontCm);
  leftLevel = std::max(leftLevel, frontLevel);
  rightLevel = std::max(rightLevel, frontLevel);

  if (waterWet && (nowMs / 400U) % 2U == 0U) {
    leftLevel = std::max<uint8_t>(leftLevel, 180);
    rightLevel = std::max<uint8_t>(rightLevel, 180);
  }

  setMotor(LEFT_MOTOR_PIN, leftLevel);
  setMotor(RIGHT_MOTOR_PIN, rightLevel);
}

uint8_t VibrationController::levelForDistance(int distanceCm) {
  if (distanceCm < 0 || distanceCm > 120) {
    return 0;
  }
  if (distanceCm <= 10) {
    return 255;
  }
  if (distanceCm <= 25) {
    return static_cast<uint8_t>(map(distanceCm, 10, 25, 255, 190));
  }
  if (distanceCm <= 60) {
    return static_cast<uint8_t>(map(distanceCm, 25, 60, 190, 100));
  }
  return static_cast<uint8_t>(map(distanceCm, 60, 120, 100, 35));
}

void VibrationController::setMotor(uint8_t pin, uint8_t level) {
  const uint8_t outputLevel = MOTOR_ACTIVE_HIGH
                                  ? level
                                  : static_cast<uint8_t>(255U - level);
  ledcWrite(pin, outputLevel);
}
