#include "DeviceController.h"

#include <Wire.h>

#include "ConfigService.h"

DeviceController::DeviceController(SensorService& sensorService, DisplayService& displayService,
                                   TimeService& timeService, MqttService& mqttService,
                                   WiFiService& wifiService,
                                   OfflineLoggerService& offlineLoggerService,
                                   PowerService& powerService, TelemetryData& telemetry)
    : sensorService_(sensorService),
      displayService_(displayService),
      timeService_(timeService),
      mqttService_(mqttService),
      wifiService_(wifiService),
      offlineLoggerService_(offlineLoggerService),
      powerService_(powerService),
      telemetry_(telemetry) {}

bool DeviceController::begin() {
  Serial.println("[BOOT] DeviceController begin");
  setState_(State::BOOT);
  bool initOk = true;

  if (!powerService_.begin()) {
    Serial.println("PowerService init failed");
    initOk = false;
  }

  // Battery mode aktif untuk mengizinkan state low-power, tanpa memaksa deep sleep periodik.
  powerService_.enableBatteryMode(true);

  if (!sensorService_.begin()) {
    Serial.println("SensorService init failed");
    initOk = false;
  }

  if (!displayService_.begin()) {
    Serial.println("DisplayService init failed");
    initOk = false;
  }

  if (!timeService_.begin()) {
    Serial.println("TimeService init failed");
    initOk = false;
  }

  if (!mqttService_.begin()) {
    Serial.println("MqttService init failed");
    initOk = false;
  }

  if (!offlineLoggerService_.begin()) {
    Serial.println("OfflineLoggerService init failed");
    initOk = false;
  }

  if (!wifiService_.begin()) {
    Serial.println("WiFiService init failed");
    initOk = false;
  }

  wifiService_.setBatteryModeEnabled(true);

  const uint32_t nowMs = millis();
  timeService_.syncFromNtpIfNeeded(telemetry_, nowMs);
  telemetry_.timestampIso = timeService_.getIsoTimestamp();
  sensorService_.update(telemetry_, nowMs);

  initialized_ = true;
  refreshState_(nowMs);
  Serial.println("[BOOT] DeviceController ready");
  return initOk;
}

void DeviceController::loop() {
  const uint32_t nowMs = millis();

  if (!initialized_) {
    return;
  }

  if (powerService_.shouldResetWifi(nowMs)) {
    wifiService_.resetCredentialsAndRestart();
    return;
  }

  handleNetworkDemand_(nowMs);
  wifiService_.loop();
  mqttService_.ensureConnection(nowMs);
  mqttService_.loop();

  handleSensorAndTime_(nowMs);
  handleDisplay_(nowMs);
  handleTelemetryPublish_(nowMs);
  refreshState_(nowMs);

  powerService_.idle();
}

DeviceController::State DeviceController::state() const {
  return state_;
}

const char* DeviceController::stateName() const {
  return stateName_(state_);
}

void DeviceController::handleDisplay_(uint32_t nowMs) {
  if (powerService_.shouldWakeDisplay(nowMs)) {
    displayService_.setEnabled(true);
    powerService_.notifyDisplayEnabled(nowMs);
  }

  if (displayService_.isEnabled() &&
      (nowMs - lastDisplayRenderMs_ >= 1000UL)) {
    lastDisplayRenderMs_ = nowMs;
    displayService_.render(telemetry_, timeService_.getDisplayTime());
  }

  if (powerService_.shouldTurnOffDisplay(nowMs)) {
    displayService_.setEnabled(false);
    powerService_.notifyDisplayDisabled();
  }
}

void DeviceController::handleNetworkDemand_(uint32_t nowMs) {
  needNtpSync_ = timeService_.shouldRequestNetwork(nowMs);
  needMqtt_ = mqttService_.shouldRequestNetwork(nowMs);
  needOfflineFlush_ = offlineLoggerService_.shouldRequestNetwork(nowMs);
  const bool networkDemand = needNtpSync_ || needMqtt_ || needOfflineFlush_;

  if (networkDemand != networkDemandActive_) {
    networkDemandActive_ = networkDemand;
    Serial.print("[STATE] Network demand ");
    Serial.print(networkDemandActive_ ? "ON" : "OFF");
    Serial.print(" ntp=");
    Serial.print(needNtpSync_ ? "1" : "0");
    Serial.print(" mqtt=");
    Serial.print(needMqtt_ ? "1" : "0");
    Serial.print(" backlog=");
    Serial.println(needOfflineFlush_ ? "1" : "0");
  }

  if (networkDemand) {
    uint32_t keepAliveMs = ConfigService::WIFI_IDLE_DISCONNECT_MS;
    if (needOfflineFlush_) {
      // Tahan WiFi sedikit lebih lama saat backlog sedang dikuras agar tidak reconnect terus.
      keepAliveMs =
          ConfigService::OFFLINE_FLUSH_INTERVAL_MS + ConfigService::WIFI_IDLE_DISCONNECT_MS;
    }
    wifiService_.requestNetwork(keepAliveMs);
  } else {
    wifiService_.releaseNetwork();
  }
}

void DeviceController::handleSensorAndTime_(uint32_t nowMs) {
  timeService_.syncFromNtpIfNeeded(telemetry_, nowMs);

  if (sensorService_.update(telemetry_, nowMs)) {
    telemetry_.timestampIso = timeService_.getIsoTimestamp();
  }
}

void DeviceController::handleTelemetryPublish_(uint32_t nowMs) {
  const bool hasBacklog = offlineLoggerService_.hasBacklog();
  if (hasBacklog) {
    // Saat backlog masih ada, sample baru diarahkan ke logger agar urutan data tetap kronologis
    // dan recovery tidak mengirim data lama setelah data baru.
    if (needMqtt_ && telemetry_.timestampIso.length() > 0 &&
        (nowMs - lastOfflineQueueMs_) >= ConfigService::MQTT_PUBLISH_INTERVAL_MS &&
        telemetry_.timestampIso != lastOfflineQueuedTimestamp_) {
      if (offlineLoggerService_.append(telemetry_)) {
        lastOfflineQueueMs_ = nowMs;
        lastOfflineQueuedTimestamp_ = telemetry_.timestampIso;
        Serial.print("[MQTT] Publish deferred, sample queued ts=");
        Serial.println(telemetry_.timestampIso);
        Serial.print("[LOGGER] queued offline ts=");
        Serial.print(telemetry_.timestampIso);
        Serial.print(" pending=");
        Serial.println(offlineLoggerService_.getPendingCount());
      }
    }

    // Replay backlog dilakukan bertahap; service logger membatasi maksimum 5 record per siklus.
    const uint8_t flushed = offlineLoggerService_.flushPending(mqttService_, nowMs);
    if (flushed > 0) {
      Serial.println("[LOGGER] backlog flush running");
      Serial.print("[LOGGER] flushed=");
      Serial.print(flushed);
      Serial.print(" pending=");
      Serial.println(offlineLoggerService_.getPendingCount());
      if (flushed == 5) {
        Serial.println("[LOGGER] flush batch cap reached");
      }
    }
    return;
  }

  // Jika publish live gagal saat due, sample diserahkan ke offline logger agar tidak hilang.
  const bool publishedLive = mqttService_.publishTelemetry(telemetry_, nowMs);
  if (needMqtt_ && publishedLive) {
    Serial.print("[MQTT] Telemetry published ts=");
    Serial.println(telemetry_.timestampIso);
  }

  if (needMqtt_ && !publishedLive && telemetry_.timestampIso.length() > 0 &&
      (nowMs - lastOfflineQueueMs_) >= ConfigService::MQTT_PUBLISH_INTERVAL_MS &&
      telemetry_.timestampIso != lastOfflineQueuedTimestamp_) {
    if (offlineLoggerService_.append(telemetry_)) {
      lastOfflineQueueMs_ = nowMs;
      lastOfflineQueuedTimestamp_ = telemetry_.timestampIso;
      Serial.print("[MQTT] Publish failed, sample queued ts=");
      Serial.println(telemetry_.timestampIso);
      Serial.print("[LOGGER] queued offline ts=");
      Serial.print(telemetry_.timestampIso);
      Serial.print(" pending=");
      Serial.println(offlineLoggerService_.getPendingCount());
    } else {
      Serial.print("[LOGGER] queue skipped ts=");
      Serial.println(telemetry_.timestampIso);
    }
  }

  if (publishedLive) {
    lastOfflineQueueMs_ = nowMs;
    lastOfflineQueuedTimestamp_ = "";
  }

  // Replay backlog dilakukan bertahap; service logger membatasi maksimum 5 record per siklus.
  const uint8_t flushed = offlineLoggerService_.flushPending(mqttService_, nowMs);
  if (flushed > 0) {
    Serial.println("[LOGGER] backlog flush running");
    Serial.print("[LOGGER] flushed=");
    Serial.print(flushed);
    Serial.print(" pending=");
    Serial.println(offlineLoggerService_.getPendingCount());
    if (flushed == 5) {
      Serial.println("[LOGGER] flush batch cap reached");
    }
  }
}

void DeviceController::refreshState_(uint32_t nowMs) {
  if (!initialized_) {
    setState_(State::BOOT);
    return;
  }

  if (wifiService_.isPortalActive() || wifiService_.isProvisioningMode()) {
    setState_(State::PROVISIONING_MODE);
    return;
  }

  const WiFiService::State wifiState = wifiService_.state();
  if (wifiState == WiFiService::State::CONNECTING ||
      wifiState == WiFiService::State::RECONNECTING) {
    setState_(State::CONNECTING_WIFI);
    return;
  }

  if (wifiService_.isConnected()) {
    // WiFi ada tetapi jalur cloud sedang rusak: tandai degraded, bukan offline penuh.
    if (mqttService_.isConfigured() &&
        (!mqttService_.isConnected() || mqttService_.hasTransportFailure() ||
         offlineLoggerService_.hasBacklog())) {
      setState_(State::DEGRADED_MODE);
      return;
    }

    setState_(State::NORMAL_OPERATION);
    return;
  }

  if (needMqtt_ || needOfflineFlush_) {
    setState_(State::OFFLINE_MODE);
    return;
  }

  if (powerService_.isBatteryModeEnabled() && !displayService_.isEnabled() &&
      !needNtpSync_ && !needMqtt_ && !needOfflineFlush_ &&
      (nowMs - sensorService_.lastReadAt()) < ConfigService::SENSOR_INTERVAL_MS) {
    setState_(State::LOW_POWER_MODE);
    return;
  }

  setState_(State::OFFLINE_MODE);
}

void DeviceController::setState_(State state) {
  if (state_ == state) {
    return;
  }

  const State previousState = state_;
  state_ = state;
  Serial.print("[STATE] Device -> ");
  Serial.println(stateName_(state_));

  switch (state_) {
    case State::NORMAL_OPERATION:
      if (previousState == State::DEGRADED_MODE || previousState == State::OFFLINE_MODE) {
        Serial.println("[STATE] Recovery complete, device back to normal");
      }
      break;
    case State::DEGRADED_MODE:
      Serial.println("[STATE] Cloud path degraded");
      break;
    case State::OFFLINE_MODE:
      Serial.println("[STATE] Device offline");
      break;
    default:
      break;
  }
}

const char* DeviceController::stateName_(State state) {
  switch (state) {
    case State::BOOT:
      return "BOOT";
    case State::CONNECTING_WIFI:
      return "CONNECTING_WIFI";
    case State::NORMAL_OPERATION:
      return "NORMAL_OPERATION";
    case State::DEGRADED_MODE:
      return "DEGRADED_MODE";
    case State::OFFLINE_MODE:
      return "OFFLINE_MODE";
    case State::PROVISIONING_MODE:
      return "PROVISIONING_MODE";
    case State::LOW_POWER_MODE:
      return "LOW_POWER_MODE";
  }

  return "UNKNOWN";
}
