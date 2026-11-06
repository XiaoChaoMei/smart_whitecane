#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>

class GpsReader {
 public:
  void begin();
  void update(uint32_t nowMs);
  bool hasFreshFix(uint32_t nowMs) const;
  double latitude();
  double longitude();
  uint32_t satellites();
  uint32_t bytesReceived() const;
  uint32_t sentencesReceived() const;
  uint32_t passedChecksum() const;
  uint32_t failedChecksum() const;
  uint32_t lastSentenceAgeMs(uint32_t nowMs) const;
  uint32_t baudRate() const;
  bool baudLocked() const;
  const char* lastSentence() const;

 private:
  HardwareSerial serial_{2};
  TinyGPSPlus gps_;
  char lastSentence_[128] = {};
  uint16_t sentenceLength_ = 0;
  uint32_t bytesReceived_ = 0;
  uint32_t sentencesReceived_ = 0;
  uint32_t lastSentenceMs_ = 0;
  uint32_t lastFixMs_ = 0;
  uint32_t currentBaud_ = 0;
  uint32_t baudStartedMs_ = 0;
  uint8_t baudIndex_ = 0;
  bool sentenceActive_ = false;
  bool baudLocked_ = false;
};
