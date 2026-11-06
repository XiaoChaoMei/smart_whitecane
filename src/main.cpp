#include <Arduino.h>

#include "config.h"
#include "control/audio.h"
#include "control/button.h"
#include "control/lighting.h"
#include "control/vibration.h"
#include "network/mqtt_service.h"
#include "sensors/bh1750.h"
#include "sensors/gps.h"
#include "sensors/ultrasonic.h"
#include "sensors/water_sensor.h"

namespace {
UltrasonicArray ultrasonic;
Bh1750LightSensor lightSensor;
LightingController lighting;
WaterSensor waterSensor;
GpsReader gps;
ButtonController button;
VibrationController vibration;
AudioController audio;
MqttService mqtt;

uint32_t lastTelemetryMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastGpsDiagMs = 0;
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Smart Guide Cane started!");
  Serial.printf("Chip: %s, CPU: %lu MHz\n", ESP.getChipModel(),
                static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  Serial.printf("SOS hold: %lu ms\n", static_cast<unsigned long>(SOS_HOLD_MS));
  Serial.println("Audio tracks: 001 front, 002 left, 003 right, 004 water,");
  Serial.println("              005 light, 006 SOS");
  Serial.printf(
      "GPS UART2: 8N1 auto-baud (start %lu), ESP RX=GPIO%u <- module TX, "
      "ESP TX=GPIO%u -> module RX\n",
      static_cast<unsigned long>(GPS_BAUD_RATE), GPS_UART_RX_PIN,
      GPS_UART_TX_PIN);
  Serial.printf("Light MOSFET: GPIO%u, active-%s, lux thresholds %.0f/%.0f\n",
                LIGHT_MOS_PIN, LIGHT_MOS_ACTIVE_HIGH ? "HIGH" : "LOW",
                LIGHT_ON_LUX, LIGHT_OFF_LUX);

  ultrasonic.begin();
  lightSensor.begin();
  lighting.begin();
  waterSensor.begin();
  gps.begin();
  button.begin();
  vibration.begin();
  audio.begin();
  mqtt.begin();

  if (!lightSensor.available()) {
    Serial.println("BH1750 not found; automatic lighting is unavailable");
  }
  lastTelemetryMs = millis();
  lastGpsDiagMs = millis();
}

void loop() {
  const uint32_t nowMs = millis();

  ultrasonic.update(nowMs);
  const DistanceReadings readings = ultrasonic.readings();
  gps.update(nowMs);
  waterSensor.update(nowMs);
  const bool lampWasOn = lighting.lampOn();
  lighting.update(nowMs, lightSensor);
  if (lighting.lampOn() != lampWasOn) {
    Serial.printf("Automatic lighting: %s, lux=%.1f, MOS GPIO%u=%s\n",
                  lighting.lampOn() ? "on" : "off", lightSensor.lux(),
                  LIGHT_MOS_PIN, lighting.mosPinHigh() ? "HIGH" : "LOW");
  }

  const ButtonEvent buttonEvent = button.update(nowMs);
  if (buttonEvent == ButtonEvent::ShortPress) {
    lighting.toggleManual();
    Serial.printf("Manual lighting: %s, MOS GPIO%u=%s\n",
                  lighting.lampOn() ? "on" : "off", LIGHT_MOS_PIN,
                  lighting.mosPinHigh() ? "HIGH" : "LOW");
  }

  // 巴法云下发的补光灯指令（主题 lamp002）
  bool cloudLampOn = false;
  if (mqtt.consumeLampCommand(cloudLampOn)) {
    lighting.setManual(cloudLampOn);
    mqtt.publishLampState(lighting.lampOn());
    Serial.printf("Cloud lamp: %s, MOS GPIO%u=%s\n",
                  lighting.lampOn() ? "on" : "off", LIGHT_MOS_PIN,
                  lighting.mosPinHigh() ? "HIGH" : "LOW");
  }

  uint8_t cloudVolume = 0;
  if (mqtt.consumeVolumeCommand(cloudVolume)) {
    audio.setVolume(cloudVolume);
    Serial.printf("Cloud audio volume: %u\n", cloudVolume);
  }

  const bool waterChanged = waterSensor.consumeChanged();
  const bool lightTurnedOn = lighting.consumeTurnedOn();
  bool promptHandled = false;

  if (buttonEvent == ButtonEvent::SosTriggered) {
    const bool gpsValid = gps.hasFreshFix(nowMs);
    audio.play(AudioPrompt::Sos, nowMs, true);
    mqtt.queueSos(nowMs, gpsValid, gps.latitude(), gps.longitude());
    Serial.println("SOS triggered");
    promptHandled = true;
  }

  if (!promptHandled && waterChanged && waterSensor.isWet()) {
    audio.play(AudioPrompt::Water, nowMs);
    promptHandled = true;
  }

  if (!promptHandled && lightTurnedOn) {
    promptHandled = audio.play(AudioPrompt::LightOn, nowMs);
  }

  if (!promptHandled && readings.frontCm > 0 &&
      readings.frontCm <= OBSTACLE_VOICE_CM) {
    promptHandled =
        audio.play(AudioPrompt::FrontObstacle, nowMs);
  }
  if (!promptHandled && readings.leftCm > 0 &&
      readings.leftCm <= OBSTACLE_VOICE_CM) {
    promptHandled = audio.play(AudioPrompt::LeftObstacle, nowMs);
  }
  if (!promptHandled && readings.rightCm > 0 &&
      readings.rightCm <= OBSTACLE_VOICE_CM) {
    audio.play(AudioPrompt::RightObstacle, nowMs);
  }

  vibration.update(readings, waterSensor.isWet(), nowMs);
  mqtt.update(nowMs);

  const bool gpsValid = gps.hasFreshFix(nowMs);
  if (mqtt.connected() &&
      nowMs - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    Telemetry telemetry{};
    telemetry.frontCm = readings.frontCm;
    telemetry.leftCm = readings.leftCm;
    telemetry.rightCm = readings.rightCm;
    telemetry.lux = lightSensor.lux();
    telemetry.waterWet = waterSensor.isWet();
    telemetry.waterRaw = waterSensor.rawValue();
    telemetry.lampOn = lighting.lampOn();
    telemetry.lampManual = lighting.manualMode();
    telemetry.volume = audio.volume();
    telemetry.gpsValid = gpsValid;
    telemetry.latitude = gps.latitude();
    telemetry.longitude = gps.longitude();
    telemetry.satellites = gps.satellites();
    telemetry.uptimeMs = nowMs;
    mqtt.publishTelemetry(telemetry);
    lastTelemetryMs = nowMs;
  }

  if (nowMs - lastLogMs >= 1000) {
    lastLogMs = nowMs;
    Serial.printf(
        "[%lu] F:%d L:%d R:%d Lux:%.1f Water:%d/%u "
        "Lamp:%d(%s) MOS:%s "
        "GPS:%s/%lu MQTT:%d\n",
        static_cast<unsigned long>(nowMs), readings.frontCm, readings.leftCm,
        readings.rightCm, lightSensor.lux(), waterSensor.isWet(),
        waterSensor.rawValue(), lighting.lampOn(),
        lighting.manualMode() ? "manual" : "auto",
        lighting.mosPinHigh() ? "HIGH" : "LOW",
        gpsValid ? "fix" : "none",
        static_cast<unsigned long>(gps.satellites()),
        mqtt.connected() ? 1 : 0);
  }

  if (nowMs - lastGpsDiagMs >= 5000) {
    lastGpsDiagMs = nowMs;
    if (gps.bytesReceived() == 0) {
      Serial.println(
          "GPS: no UART bytes. Check module TX -> J3 pin3 (GPIO26), common "
          "GND and module power.");
    } else if (!gps.baudLocked()) {
      Serial.printf(
          "GPS: %lu bytes, no valid NMEA. Baud probe is at %lu; check UART "
          "voltage and that module TX is not connected to ESP TX.\n",
          static_cast<unsigned long>(gps.bytesReceived()),
          static_cast<unsigned long>(gps.baudRate()));
    } else if (!gps.hasFreshFix(nowMs)) {
      Serial.printf(
          "GPS: NMEA rx=%lu lines=%lu checksum=%lu/%lu, no fix. Go outdoors "
          "with the antenna connected. Last: %s\n",
          static_cast<unsigned long>(gps.bytesReceived()),
          static_cast<unsigned long>(gps.sentencesReceived()),
          static_cast<unsigned long>(gps.passedChecksum()),
          static_cast<unsigned long>(gps.failedChecksum()),
          gps.lastSentence());
    } else {
      Serial.printf(
          "GPS fix: %.6f, %.6f, satellites=%lu, last NMEA age=%lu ms\n",
          gps.latitude(), gps.longitude(),
          static_cast<unsigned long>(gps.satellites()),
          static_cast<unsigned long>(gps.lastSentenceAgeMs(nowMs)));
    }
  }

  delay(2);
}
