#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID ""
#endif

// Keep every GPIO here so wiring and firmware cannot drift apart.
constexpr uint8_t FRONT_TRIG_PIN = 16;
constexpr uint8_t FRONT_ECHO_PIN = 34;
constexpr uint8_t LEFT_TRIG_PIN = 17;
constexpr uint8_t LEFT_ECHO_PIN = 35;
constexpr uint8_t RIGHT_TRIG_PIN = 18;
constexpr uint8_t RIGHT_ECHO_PIN = 36;

constexpr uint8_t BH1750_SDA_PIN = 21;
constexpr uint8_t BH1750_SCL_PIN = 22;
// ESP32 RX <- GPS TX, ESP32 TX -> GPS RX. The supplied examples use 9600 8N1.
constexpr uint8_t GPS_UART_RX_PIN = 26;
constexpr uint8_t GPS_UART_TX_PIN = 27;
constexpr uint32_t GPS_BAUD_RATE = 9600;
constexpr uint32_t GPS_FIX_MAX_AGE_MS = 20000;
constexpr uint8_t DFPLAYER_RX_PIN = 32;
constexpr uint8_t DFPLAYER_TX_PIN = 33;

constexpr uint8_t LEFT_MOTOR_PIN = 13;
constexpr uint8_t RIGHT_MOTOR_PIN = 14;
// GPIO39 is ADC1 and remains available while Wi-Fi is active.
constexpr uint8_t WATER_SENSOR_PIN = 39;
constexpr uint8_t LIGHT_MOS_PIN = 25;
constexpr uint8_t BUTTON_PIN = 23;

constexpr uint32_t ULTRASONIC_ECHO_TIMEOUT_US = 30000;
constexpr uint32_t ULTRASONIC_START_INTERVAL_MS = 60;
constexpr int OBSTACLE_VOICE_CM = 25;

constexpr float LIGHT_ON_LUX = 80.0F;
constexpr float LIGHT_OFF_LUX = 150.0F;
constexpr uint32_t LIGHT_SAMPLE_INTERVAL_MS = 500;

constexpr uint32_t WATER_DEBOUNCE_MS = 500;
constexpr uint16_t WATER_WET_ADC = 1800;
constexpr uint16_t WATER_DRY_ADC = 1200;
constexpr bool LIGHT_MOS_ACTIVE_HIGH = true;
constexpr bool MOTOR_ACTIVE_HIGH = true;

constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;
constexpr uint32_t SOS_HOLD_MS = 3000;

constexpr uint8_t DFPLAYER_VOLUME = 20;
constexpr uint8_t DFPLAYER_MAX_VOLUME = 30;
constexpr uint32_t AUDIO_COOLDOWN_MS = 3000;

constexpr char DEVICE_ID[] = "cane-demo";

// ===== 巴法云 MQTT =====
// 服务器 bemfa.com，9501 是明文端口（9503 为 TLS，本项目不加密）
constexpr char MQTT_HOST[] = "bemfa.com";
constexpr uint16_t MQTT_PORT = 9501;

#define MQTT_TOPIC_OBSTACLE "bz004"  // 避障：三路超声波距离
#define MQTT_TOPIC_WATER "bs004"     // 避水：水浸检测
#define MQTT_TOPIC_LIGHT "yd004"     // 光照 / 夜灯状态
#define MQTT_TOPIC_GPS "dw004"       // GPS 定位
// 这两个需要你在巴法云控制台新建：
#define MQTT_TOPIC_SOS "sos006"   // 开关：SOS 告警
#define MQTT_TOPIC_LAMP "lamp002"  // 灯泡：远程控制补光灯（设备订阅）

constexpr uint32_t TELEMETRY_INTERVAL_MS = 5000;
