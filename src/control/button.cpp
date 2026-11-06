#include "control/button.h"

#include "config.h"

void ButtonController::begin() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  rawPressed_ = digitalRead(BUTTON_PIN) == LOW;
  stablePressed_ = rawPressed_;
  rawChangedMs_ = millis();
  pressedMs_ = rawChangedMs_;
}

ButtonEvent ButtonController::update(uint32_t nowMs) {
  const bool sampledPressed = digitalRead(BUTTON_PIN) == LOW;
  if (sampledPressed != rawPressed_) {
    rawPressed_ = sampledPressed;
    rawChangedMs_ = nowMs;
  }

  if (rawPressed_ != stablePressed_ &&
      nowMs - rawChangedMs_ >= BUTTON_DEBOUNCE_MS) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressedMs_ = nowMs;
      sosSent_ = false;
    } else if (!sosSent_) {
      return ButtonEvent::ShortPress;
    }
  }

  if (stablePressed_ && !sosSent_ && nowMs - pressedMs_ >= SOS_HOLD_MS) {
    sosSent_ = true;
    return ButtonEvent::SosTriggered;
  }

  return ButtonEvent::None;
}
