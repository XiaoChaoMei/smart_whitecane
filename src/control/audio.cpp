#include "control/audio.h"

#include "config.h"

AudioController::AudioController() : volume_(DFPLAYER_VOLUME) {}

void AudioController::begin() {
  serial_.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(1200);
  setVolume(DFPLAYER_VOLUME);
}

void AudioController::setVolume(uint8_t volume) {
  volume_ = volume > DFPLAYER_MAX_VOLUME ? DFPLAYER_MAX_VOLUME : volume;
  sendCommand(0x06, volume_);
}

uint8_t AudioController::volume() const {
  return volume_;
}

bool AudioController::play(AudioPrompt prompt, uint32_t nowMs, bool force) {
  const auto index = static_cast<uint8_t>(prompt);
  if (!force && played_[index] &&
      nowMs - lastPlayedMs_[index] < AUDIO_COOLDOWN_MS) {
    return false;
  }

  const uint16_t folderAndTrack =
      static_cast<uint16_t>((1U << 8U) | trackFor(prompt));
  sendCommand(0x0F, folderAndTrack);
  lastPlayedMs_[index] = nowMs;
  played_[index] = true;
  return true;
}

uint8_t AudioController::trackFor(AudioPrompt prompt) {
  return static_cast<uint8_t>(prompt) + 1U;
}

void AudioController::sendCommand(uint8_t command, uint16_t parameter) {
  uint8_t frame[10] = {0x7E, 0xFF, 0x06, command, 0x00,
                       static_cast<uint8_t>(parameter >> 8U),
                       static_cast<uint8_t>(parameter & 0xFFU), 0, 0, 0xEF};
  uint16_t checksum = 0;
  for (uint8_t index = 1; index <= 6; ++index) {
    checksum += frame[index];
  }
  checksum = static_cast<uint16_t>(0U - checksum);
  frame[7] = static_cast<uint8_t>(checksum >> 8U);
  frame[8] = static_cast<uint8_t>(checksum & 0xFFU);
  serial_.write(frame, sizeof(frame));
}
