#include "sensors/bh1750.h"

#include <Wire.h>

#include "config.h"

void Bh1750LightSensor::begin() {
  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);

  for (const uint8_t candidate : {0x23, 0x5C}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address_ = candidate;
      break;
    }
  }

  if (address_ == 0) {
    return;
  }

  writeCommand(0x01);
  delay(10);
  writeCommand(0x10);
  delay(180);
}

bool Bh1750LightSensor::update(uint32_t nowMs) {
  if (address_ == 0 || nowMs - lastSampleMs_ < LIGHT_SAMPLE_INTERVAL_MS) {
    return false;
  }
  lastSampleMs_ = nowMs;

  if (Wire.requestFrom(address_, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  const uint16_t raw =
      static_cast<uint16_t>(Wire.read() << 8U) | static_cast<uint8_t>(Wire.read());
  lux_ = static_cast<float>(raw) / 1.2F;
  return true;
}

bool Bh1750LightSensor::available() const {
  return address_ != 0;
}

float Bh1750LightSensor::lux() const {
  return lux_;
}

bool Bh1750LightSensor::writeCommand(uint8_t command) const {
  Wire.beginTransmission(address_);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}
