#include "sensors/water_sensor.h"

#include "config.h"

void WaterSensor::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(WATER_SENSOR_PIN, ADC_11db);
  pinMode(WATER_SENSOR_PIN, INPUT);
  rawValue_ = analogRead(WATER_SENSOR_PIN);
  rawWet_ = rawValue_ >= WATER_WET_ADC;
  candidateWet_ = rawWet_;
  stableWet_ = rawWet_;
  candidateSinceMs_ = millis();
}

void WaterSensor::update(uint32_t nowMs) {
  rawValue_ = analogRead(WATER_SENSOR_PIN);
  const bool sampledWet =
      rawValue_ >= (stableWet_ ? WATER_DRY_ADC : WATER_WET_ADC);
  if (sampledWet != rawWet_) {
    rawWet_ = sampledWet;
    candidateWet_ = sampledWet;
    candidateSinceMs_ = nowMs;
    return;
  }

  if (candidateWet_ != stableWet_ &&
      nowMs - candidateSinceMs_ >= WATER_DEBOUNCE_MS) {
    stableWet_ = candidateWet_;
    changed_ = true;
  }
}

bool WaterSensor::isWet() const {
  return stableWet_;
}

bool WaterSensor::consumeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

uint16_t WaterSensor::rawValue() const {
  return rawValue_;
}
