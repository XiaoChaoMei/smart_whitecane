#pragma once

#include <Arduino.h>

enum class AudioPrompt : uint8_t {
  FrontObstacle,
  LeftObstacle,
  RightObstacle,
  Water,
  LightOn,
  Sos,
  Count,
};

class AudioController {
 public:
  AudioController();
  void begin();
  bool play(AudioPrompt prompt, uint32_t nowMs, bool force = false);
  void setVolume(uint8_t volume);
  uint8_t volume() const;

 private:
  static uint8_t trackFor(AudioPrompt prompt);
  void sendCommand(uint8_t command, uint16_t parameter);

  HardwareSerial serial_{1};
  uint8_t volume_ = 0;
  uint32_t lastPlayedMs_[static_cast<uint8_t>(AudioPrompt::Count)] = {};
  bool played_[static_cast<uint8_t>(AudioPrompt::Count)] = {};
};
