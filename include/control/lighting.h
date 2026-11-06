#pragma once

#include <Arduino.h>

#include "sensors/bh1750.h"

class LightingController {
 public:
  void begin();
  void update(uint32_t nowMs, Bh1750LightSensor& sensor);
  void toggleManual();
  // 云端/按键指定开或关，并切到手动模式（不再按光照自动判断）
  void setManual(bool on);
  bool lampOn() const;
  bool manualMode() const;
  bool mosPinHigh() const;
  bool consumeTurnedOn();

 private:
  void setLamp(bool on);

  bool lampOn_ = false;
  bool manualMode_ = false;
  bool turnedOn_ = false;
};
