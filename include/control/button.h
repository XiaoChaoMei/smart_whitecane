#pragma once

#include <Arduino.h>

enum class ButtonEvent : uint8_t {
  None,
  ShortPress,
  SosTriggered,
};

class ButtonController {
 public:
  void begin();
  ButtonEvent update(uint32_t nowMs);

 private:
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool sosSent_ = false;
  uint32_t rawChangedMs_ = 0;
  uint32_t pressedMs_ = 0;
};
