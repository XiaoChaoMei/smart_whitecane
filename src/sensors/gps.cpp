#include "sensors/gps.h"

#include <cstdlib>
#include <cstring>

#include "config.h"

namespace {
constexpr uint32_t GPS_BAUD_RATES[] = {
    GPS_BAUD_RATE, 4800, 38400, 57600, 115200};
constexpr uint32_t GPS_BAUD_PROBE_MS = 4000;

bool isFixSentence(const char* sentence) {
  if (!sentence || sentence[0] != '$') {
    return false;
  }

  const char* checksumMark = std::strchr(sentence, '*');
  if (!checksumMark || std::strlen(checksumMark) < 3) {
    return false;
  }

  uint8_t checksum = 0;
  for (const char* current = sentence + 1; current < checksumMark; ++current) {
    checksum ^= static_cast<uint8_t>(*current);
  }
  char expectedText[3] = {checksumMark[1], checksumMark[2], '\0'};
  char* end = nullptr;
  const unsigned long expected = std::strtoul(expectedText, &end, 16);
  if (end != expectedText + 2 || expected != checksum) {
    return false;
  }

  const char* comma = std::strchr(sentence, ',');
  if (!comma || comma - sentence < 6) {
    return false;
  }
  const char* type = comma - 3;
  return std::strncmp(type, "GGA", 3) == 0 ||
         std::strncmp(type, "RMC", 3) == 0;
}
}  // namespace

void GpsReader::begin() {
  serial_.setRxBufferSize(512);
  currentBaud_ = GPS_BAUD_RATES[0];
  serial_.begin(currentBaud_, SERIAL_8N1, GPS_UART_RX_PIN,
                GPS_UART_TX_PIN);
  baudStartedMs_ = millis();
}

void GpsReader::update(uint32_t nowMs) {
  uint16_t processed = 0;
  while (serial_.available() > 0 && processed < 128) {
    const char value = static_cast<char>(serial_.read());
    ++bytesReceived_;
    gps_.encode(value);

    if (value == '$') {
      sentenceLength_ = 1;
      sentenceActive_ = true;
      lastSentence_[0] = '$';
    } else if (sentenceActive_) {
      if (value == '\r' || value == '\n') {
        if (sentenceLength_ > 1) {
          lastSentence_[sentenceLength_] = '\0';
          ++sentencesReceived_;
          lastSentenceMs_ = nowMs;
          if (!baudLocked_ && isFixSentence(lastSentence_)) {
            baudLocked_ = true;
            Serial.printf("GPS baud detected: %lu\n",
                          static_cast<unsigned long>(currentBaud_));
          }
        }
        sentenceActive_ = false;
      } else if (sentenceLength_ + 1 < sizeof(lastSentence_)) {
        lastSentence_[sentenceLength_++] = value;
      } else {
        sentenceActive_ = false;
      }
    }
    ++processed;
  }

  if (!baudLocked_ && nowMs - baudStartedMs_ >= GPS_BAUD_PROBE_MS) {
    baudIndex_ =
        static_cast<uint8_t>((baudIndex_ + 1U) %
                             (sizeof(GPS_BAUD_RATES) / sizeof(uint32_t)));
    currentBaud_ = GPS_BAUD_RATES[baudIndex_];
    serial_.end();
    serial_.begin(currentBaud_, SERIAL_8N1, GPS_UART_RX_PIN,
                  GPS_UART_TX_PIN);
    sentenceActive_ = false;
    sentenceLength_ = 0;
    baudStartedMs_ = nowMs;
    Serial.printf("GPS baud probe: %lu\n",
                  static_cast<unsigned long>(currentBaud_));
  }

  if (gps_.location.isUpdated() && gps_.location.isValid()) {
    lastFixMs_ = nowMs;
  }
}

bool GpsReader::hasFreshFix(uint32_t nowMs) const {
  return lastFixMs_ != 0 && nowMs - lastFixMs_ <= GPS_FIX_MAX_AGE_MS;
}

double GpsReader::latitude() {
  return gps_.location.lat();
}

double GpsReader::longitude() {
  return gps_.location.lng();
}

uint32_t GpsReader::satellites() {
  return gps_.satellites.isValid() ? gps_.satellites.value() : 0;
}

uint32_t GpsReader::bytesReceived() const {
  return bytesReceived_;
}

uint32_t GpsReader::sentencesReceived() const {
  return sentencesReceived_;
}

uint32_t GpsReader::passedChecksum() const {
  return gps_.passedChecksum();
}

uint32_t GpsReader::failedChecksum() const {
  return gps_.failedChecksum();
}

uint32_t GpsReader::lastSentenceAgeMs(uint32_t nowMs) const {
  return lastSentenceMs_ == 0 ? UINT32_MAX : nowMs - lastSentenceMs_;
}

uint32_t GpsReader::baudRate() const {
  return currentBaud_;
}

bool GpsReader::baudLocked() const {
  return baudLocked_;
}

const char* GpsReader::lastSentence() const {
  return lastSentence_;
}
