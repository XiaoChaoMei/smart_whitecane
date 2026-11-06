#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

struct Telemetry {
  int frontCm;
  int leftCm;
  int rightCm;
  float lux;
  bool waterWet;
  uint16_t waterRaw;
  bool lampOn;
  bool lampManual;
  uint8_t volume;
  bool gpsValid;
  double latitude;
  double longitude;
  uint32_t satellites;
  uint32_t uptimeMs;
};

class MqttService {
 public:
  MqttService();

  void begin();
  void update(uint32_t nowMs);
  bool publishTelemetry(const Telemetry& telemetry);
  bool publishLampState(bool on);
  void queueSos(uint32_t nowMs, bool gpsValid, double latitude,
                double longitude);
  bool consumeVolumeCommand(uint8_t& volume);
  // 云端下发过补光灯指令时返回 true，并带出要开还是关
  bool consumeLampCommand(bool& on);
  bool connected();

 private:
  bool connectMqtt(uint32_t nowMs);
  bool publishPendingSos(uint32_t nowMs);
  bool publishJson(const char* topic, const char* payload);
  bool publishTelemetryJson(const char* upTopic, const char* pushTopic,
                            const char* payload);
  void handleIncoming(char* topic, uint8_t* payload, unsigned int length);
  void dumpWifiScan();

  WiFiClient networkClient_;
  PubSubClient mqttClient_;
  char clientId_[48] = {};
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastMqttAttemptMs_ = 0;
  uint32_t lastSosAttemptMs_ = 0;
  uint32_t sosUptimeMs_ = 0;
  double sosLatitude_ = 0.0;
  double sosLongitude_ = 0.0;
  bool sosGpsValid_ = false;
  bool sosPending_ = false;
  bool wifiConfigured_ = false;
  bool lampCommandPending_ = false;
  bool lampCommandOn_ = false;
  bool volumeCommandPending_ = false;
  uint8_t volumeCommandValue_ = 0;
  uint8_t wifiRetryCount_ = 0;
  bool scanPrinted_ = false;
};
