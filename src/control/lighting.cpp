#include "control/lighting.h"

#include "config.h"

void LightingController::begin() {
  const uint8_t inactiveLevel = LIGHT_MOS_ACTIVE_HIGH ? LOW : HIGH;
  digitalWrite(LIGHT_MOS_PIN, inactiveLevel);
  pinMode(LIGHT_MOS_PIN, OUTPUT);
  setLamp(false);
}

void LightingController::update(uint32_t nowMs, Bh1750LightSensor& sensor) {
  sensor.update(nowMs);
  if (manualMode_ || !sensor.available()) {
    return;
  }

  const float lux = sensor.lux();
  if (!lampOn_ && lux >= 0.0F && lux < LIGHT_ON_LUX) {
    setLamp(true);
  } else if (lampOn_ && lux >= LIGHT_OFF_LUX) {
    setLamp(false);
  }
}

void LightingController::toggleManual() {
  manualMode_ = true;
  setLamp(!lampOn_);
}

void LightingController::setManual(bool on) {
  manualMode_ = true;
  setLamp(on);
}

bool LightingController::lampOn() const {
  return lampOn_;
}

bool LightingController::manualMode() const {
  return manualMode_;
}

bool LightingController::mosPinHigh() const {
  return digitalRead(LIGHT_MOS_PIN) == HIGH;
}

bool LightingController::consumeTurnedOn() {
  const bool turnedOn = turnedOn_;
  turnedOn_ = false;
  return turnedOn;
}

void LightingController::setLamp(bool on) {
  lampOn_ = on;
  const bool outputHigh = LIGHT_MOS_ACTIVE_HIGH ? on : !on;
  digitalWrite(LIGHT_MOS_PIN, outputHigh ? HIGH : LOW);
  if (on) {
    turnedOn_ = true;
  }
}
