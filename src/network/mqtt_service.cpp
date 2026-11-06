#include "network/mqtt_service.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

namespace {
// 巴法云：/up 只更新云端数据，/set 会推送给所有订阅者
constexpr char OBSTACLE_TOPIC[] = MQTT_TOPIC_OBSTACLE "/up";
constexpr char OBSTACLE_PUSH_TOPIC[] = MQTT_TOPIC_OBSTACLE "/set";
constexpr char WATER_TOPIC[] = MQTT_TOPIC_WATER "/up";
constexpr char WATER_PUSH_TOPIC[] = MQTT_TOPIC_WATER "/set";
constexpr char LIGHT_TOPIC[] = MQTT_TOPIC_LIGHT "/up";
constexpr char LIGHT_PUSH_TOPIC[] = MQTT_TOPIC_LIGHT "/set";
constexpr char GPS_TOPIC[] = MQTT_TOPIC_GPS "/up";
constexpr char GPS_PUSH_TOPIC[] = MQTT_TOPIC_GPS "/set";
constexpr char SOS_TOPIC[] = MQTT_TOPIC_SOS "/set";
constexpr char LAMP_STATE_TOPIC[] = MQTT_TOPIC_LAMP "/up";

// 把距离写成 "25 cm" 或 "无"
void formatCm(int cm, char* out, size_t size) {
  if (cm <= 0) {
    std::snprintf(out, size, "无");
  } else {
    std::snprintf(out, size, "%d cm", cm);
  }
}
}  // namespace

MqttService::MqttService() : mqttClient_(networkClient_) {}

void MqttService::begin() {
  mqttClient_.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient_.setKeepAlive(15);
  mqttClient_.setBufferSize(768);
  mqttClient_.setCallback(
      [this](char* topic, uint8_t* payload, unsigned int length) {
        handleIncoming(topic, payload, length);
      });

  if (MQTT_CLIENT_ID[0] != '\0') {
    // 巴法云：client_id 必须等于账号私钥
    std::snprintf(clientId_, sizeof(clientId_), "%s", MQTT_CLIENT_ID);
  } else {
    const uint64_t chipId = ESP.getEfuseMac();
    std::snprintf(clientId_, sizeof(clientId_), "cane-%06lx",
                  static_cast<unsigned long>(chipId & 0xFFFFFFULL));
  }

  wifiConfigured_ = WIFI_SSID[0] != '\0';
  if (!wifiConfigured_) {
    Serial.println("MQTT disabled: fill in include/secrets.h");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs_ = millis();
  Serial.printf("Wi-Fi connecting to %s\n", WIFI_SSID);
}

void MqttService::update(uint32_t nowMs) {
  if (!wifiConfigured_) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (nowMs - lastWifiAttemptMs_ >= 10000) {
      lastWifiAttemptMs_ = nowMs;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      ++wifiRetryCount_;
      Serial.printf("Wi-Fi retry %s (#%u)\n", WIFI_SSID, wifiRetryCount_);
      if (wifiRetryCount_ >= 3 && !scanPrinted_) {
        scanPrinted_ = true;
        dumpWifiScan();
      }
    }
    return;
  }

  if (!mqttClient_.connected()) {
    if (nowMs - lastMqttAttemptMs_ >= 5000) {
      connectMqtt(nowMs);
    }
    return;
  }

  mqttClient_.loop();
  publishPendingSos(nowMs);
}

bool MqttService::publishJson(const char* topic, const char* payload) {
  return mqttClient_.publish(
      topic, reinterpret_cast<const uint8_t*>(payload),
      static_cast<unsigned int>(std::strlen(payload)), false);
}

bool MqttService::publishTelemetryJson(const char* upTopic,
                                       const char* pushTopic,
                                       const char* payload) {
  // /up 保存云端数据，/set 实时推送小程序；两条都发才能兼顾两端。
  const bool stored = publishJson(upTopic, payload);
  const bool pushed = publishJson(pushTopic, payload);
  return stored && pushed;
}

bool MqttService::publishTelemetry(const Telemetry& telemetry) {
  if (!mqttClient_.connected()) {
    return false;
  }

  char payload[320];
  char front[16];
  char left[16];
  char right[16];
  formatCm(telemetry.frontCm, front, sizeof(front));
  formatCm(telemetry.leftCm, left, sizeof(left));
  formatCm(telemetry.rightCm, right, sizeof(right));

  // 找最近的一路，判断是否进入警戒距离
  int nearestCm = -1;
  const char* nearestDir = "无";
  const int candidates[3] = {telemetry.frontCm, telemetry.leftCm,
                             telemetry.rightCm};
  const char* names[3] = {"前", "左", "右"};
  for (int i = 0; i < 3; ++i) {
    if (candidates[i] > 0 && (nearestCm < 0 || candidates[i] < nearestCm)) {
      nearestCm = candidates[i];
      nearestDir = names[i];
    }
  }
  const bool detected = nearestCm > 0 && nearestCm <= OBSTACLE_VOICE_CM;

  std::snprintf(
      payload, sizeof(payload),
      "{\"检测状态\":\"%s\",\"前\":\"%s\",\"左\":\"%s\",\"右\":\"%s\","
      "\"最近方向\":\"%s\",\"最近距离\":\"%d cm\",\"检测提示\":\"%s\"}",
      detected ? "已检测" : "未检测", front, left, right, nearestDir,
      nearestCm > 0 ? nearestCm : 0,
      detected ? "已进入警戒距离，请注意" : "前方安全");
  bool ok =
      publishTelemetryJson(OBSTACLE_TOPIC, OBSTACLE_PUSH_TOPIC, payload);

  std::snprintf(payload, sizeof(payload),
                "{\"检测状态\":\"%s\",\"水浸原始值\":%u,\"检测提示\":\"%s\"}",
                telemetry.waterWet ? "已检测" : "未检测",
                static_cast<unsigned int>(telemetry.waterRaw),
                telemetry.waterWet ? "检测到积水，请绕行" : "路面干燥");
  ok = publishTelemetryJson(WATER_TOPIC, WATER_PUSH_TOPIC, payload) && ok;

  std::snprintf(payload, sizeof(payload),
                "{\"光照强度\":\"%.1f lux\",\"夜灯状态\":\"%s\","
                "\"模式\":\"%s\",\"音量\":%u}",
                telemetry.lux, telemetry.lampOn ? "已开启" : "已关闭",
                telemetry.lampManual ? "手动" : "自动",
                static_cast<unsigned int>(telemetry.volume));
  ok = publishTelemetryJson(LIGHT_TOPIC, LIGHT_PUSH_TOPIC, payload) && ok;

  if (telemetry.gpsValid) {
    std::snprintf(
        payload, sizeof(payload),
        "{\"status\":\"已定位\",\"纬度\":\"%.6f\",\"经度\":\"%.6f\","
        "\"卫星\":%lu}",
        telemetry.latitude, telemetry.longitude,
        static_cast<unsigned long>(telemetry.satellites));
  } else {
    std::snprintf(payload, sizeof(payload),
                  "{\"status\":\"未定位\",\"纬度\":\"-\",\"经度\":\"-\","
                  "\"卫星\":%lu}",
                  static_cast<unsigned long>(telemetry.satellites));
  }
  ok = publishTelemetryJson(GPS_TOPIC, GPS_PUSH_TOPIC, payload) && ok;

  return ok;
}

bool MqttService::publishLampState(bool on) {
  if (!mqttClient_.connected()) {
    return false;
  }
  const char* message = on ? "on" : "off";
  return mqttClient_.publish(LAMP_STATE_TOPIC,
                             reinterpret_cast<const uint8_t*>(message),
                             static_cast<unsigned int>(std::strlen(message)),
                             true);
}

void MqttService::queueSos(uint32_t nowMs, bool gpsValid, double latitude,
                           double longitude) {
  sosPending_ = true;
  sosUptimeMs_ = nowMs;
  sosGpsValid_ = gpsValid;
  sosLatitude_ = latitude;
  sosLongitude_ = longitude;
  lastSosAttemptMs_ = 0;
  publishPendingSos(nowMs);
}

bool MqttService::consumeLampCommand(bool& on) {
  if (!lampCommandPending_) {
    return false;
  }
  on = lampCommandOn_;
  lampCommandPending_ = false;
  return true;
}

bool MqttService::consumeVolumeCommand(uint8_t& volume) {
  if (!volumeCommandPending_) {
    return false;
  }
  volume = volumeCommandValue_;
  volumeCommandPending_ = false;
  return true;
}

bool MqttService::connected() {
  return mqttClient_.connected();
}

void MqttService::handleIncoming(char* topic, uint8_t* payload,
                                 unsigned int length) {
  if (!topic) {
    return;
  }
  // 巴法云可能以 "lamp002" 或 "lamp002/set" 投递，前缀匹配即可
  if (std::strncmp(topic, MQTT_TOPIC_LAMP,
                   std::strlen(MQTT_TOPIC_LAMP)) != 0) {
    return;
  }

  char buffer[16] = {};
  const unsigned int copyLength =
      length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;
  for (unsigned int i = 0; i < copyLength; ++i) {
    buffer[i] = static_cast<char>(std::tolower(payload[i]));
  }

  if (std::strncmp(buffer, "vol:", 4) == 0) {
    char* end = nullptr;
    const long volume = std::strtol(buffer + 4, &end, 10);
    if (end == buffer + 4 || *end != '\0' || volume < 0 ||
        volume > DFPLAYER_MAX_VOLUME) {
      Serial.printf("MQTT volume: invalid command \"%s\"\n", buffer);
      return;
    }
    volumeCommandValue_ = static_cast<uint8_t>(volume);
    volumeCommandPending_ = true;
    Serial.printf("MQTT volume: %u\n", volumeCommandValue_);
    return;
  }

  bool on;
  if (std::strcmp(buffer, "on") == 0 || std::strcmp(buffer, "1") == 0) {
    on = true;
  } else if (std::strcmp(buffer, "off") == 0 || std::strcmp(buffer, "0") == 0) {
    on = false;
  } else {
    Serial.printf("MQTT lamp: unrecognized command \"%s\"\n", buffer);
    return;
  }

  lampCommandOn_ = on;
  lampCommandPending_ = true;
  Serial.printf("MQTT lamp command: %s\n", on ? "on" : "off");
}

void MqttService::dumpWifiScan() {
  Serial.println("--- Wi-Fi scan (ESP32 只能看到 2.4GHz) ---");
  const int found = WiFi.scanNetworks();
  if (found <= 0) {
    Serial.println("  没扫到任何 Wi-Fi");
    return;
  }
  for (int i = 0; i < found; ++i) {
    Serial.printf("  [%d] %-24s %4d dBm  ch%-3d %s\n", i,
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "enc");
  }
  WiFi.scanDelete();
  Serial.println("--- scan end ---");
}

bool MqttService::connectMqtt(uint32_t nowMs) {
  lastMqttAttemptMs_ = nowMs;

  const char* user = MQTT_USER[0] != '\0' ? MQTT_USER : nullptr;
  const char* password = MQTT_USER[0] != '\0' ? MQTT_PASSWORD : nullptr;

  bool connectedNow;
  if (user) {
    connectedNow = mqttClient_.connect(clientId_, user, password);
  } else {
    connectedNow = mqttClient_.connect(clientId_);
  }

  if (!connectedNow) {
    Serial.printf("MQTT connect failed, state=%d\n", mqttClient_.state());
    return false;
  }

  Serial.printf("MQTT connected %s:%u as %s\n", MQTT_HOST, MQTT_PORT, clientId_);
  mqttClient_.subscribe(MQTT_TOPIC_LAMP, 1);
  Serial.printf("MQTT subscribed %s\n", MQTT_TOPIC_LAMP);
  publishPendingSos(nowMs);
  return true;
}

bool MqttService::publishPendingSos(uint32_t nowMs) {
  if (!sosPending_ || !mqttClient_.connected() ||
      nowMs - lastSosAttemptMs_ < 5000) {
    return false;
  }
  lastSosAttemptMs_ = nowMs;

  char payload[256];
  const int length = std::snprintf(
      payload, sizeof(payload),
      "{\"求救\":\"触发\",\"设备\":\"%s\",\"运行时长\":\"%lu s\","
      "\"定位\":\"%s\",\"纬度\":\"%.6f\",\"经度\":\"%.6f\"}",
      DEVICE_ID, static_cast<unsigned long>(sosUptimeMs_ / 1000),
      sosGpsValid_ ? "已定位" : "未定位", sosLatitude_, sosLongitude_);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(payload)) {
    return false;
  }

  const bool published =
      mqttClient_.publish(SOS_TOPIC, reinterpret_cast<const uint8_t*>(payload),
                          static_cast<unsigned int>(length), false);
  if (published) {
    sosPending_ = false;
    Serial.println("SOS published");
  }
  return published;
}
