#include "MqttService.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "ConfigService.h"

namespace {

MqttService* g_activeMqttService = nullptr;

}  // namespace

bool MqttService::begin() {
  // Broker dikonfigurasi sekali saat startup.
  mqttClient_.setClient(wifiClient_);
  mqttClient_.setCallback(MqttService::mqttCallback_);
  g_activeMqttService = this;
  initialized_ = true;
  reconnectBackoffMs_ = ConfigService::MQTT_RECONNECT_INTERVAL_MS;
  transportFailure_ = false;
  Serial.println("[MQTT] Service ready");
  return true;
}

void MqttService::ensureConnection(uint32_t nowMs) {
  // WiFi dan MQTT direconnect berkala agar loop utama tetap ringan.
  if (!initialized_) {
    return;
  }

  const char* mqttHost = ConfigService::getMqttHost();
  const uint16_t mqttPort = ConfigService::getMqttPort();
  const char* mqttUsername = ConfigService::getMqttUsername();
  const char* mqttPassword = ConfigService::getMqttPassword();
  if (mqttHost == nullptr || mqttHost[0] == '\0' || mqttPort == 0) {
    return;
  }

  mqttClient_.setServer(mqttHost, mqttPort);

  if (mqttClient_.connected() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if ((nowMs - lastReconnectAttemptMs_) < reconnectBackoffMs_) {
    return;
  }

  lastReconnectAttemptMs_ = nowMs;
  const String statusTopic = buildStatusTopic_();
  const char* deviceId = ConfigService::getDeviceId();
  if (deviceId == nullptr || deviceId[0] == '\0') {
    return;
  }

  Serial.print("[MQTT] Connect broker=");
  Serial.print(mqttHost);
  Serial.print(":");
  Serial.println(mqttPort);

  const bool connected = mqttClient_.connect(deviceId, mqttUsername, mqttPassword,
                                             statusTopic.c_str(), 0, true, "offline");

  if (connected) {
    const uint32_t previousBackoffMs = reconnectBackoffMs_;
    subscribeTopics_();
    mqttClient_.publish(statusTopic.c_str(), "online", true);
    reconnectBackoffMs_ = ConfigService::MQTT_RECONNECT_INTERVAL_MS;
    transportFailure_ = false;
    Serial.println("[MQTT] Connected");
    if (previousBackoffMs > ConfigService::MQTT_RECONNECT_INTERVAL_MS) {
      Serial.print("[MQTT] Recovered after backoff ms=");
      Serial.println(previousBackoffMs);
    }
  } else {
    transportFailure_ = true;
    reconnectBackoffMs_ =
        min(static_cast<uint32_t>(reconnectBackoffMs_ * 2UL), ConfigService::WIFI_MAX_BACKOFF_MS);
    Serial.print("[MQTT] Connect failed rc=");
    Serial.println(mqttClient_.state());
    Serial.print("[MQTT] Retry backoff ms=");
    Serial.println(reconnectBackoffMs_);
  }
}

bool MqttService::publishTelemetry(const TelemetryData& telemetry, uint32_t nowMs) {
  if (!initialized_ || !isConfigured()) {
    return false;
  }

  if ((nowMs - lastPublishMs_) < ConfigService::MQTT_PUBLISH_INTERVAL_MS) {
    return false;
  }

  const String payload = buildPayload_(telemetry);
  return publishPayload(payload, nowMs);
}

bool MqttService::publishPayload(const String& payload, uint32_t nowMs) {
  // Publish dijeda agar baterai dan trafik tetap efisien.
  if (!initialized_ || !mqttClient_.connected()) {
    return false;
  }

  if ((nowMs - lastPublishMs_) < ConfigService::MQTT_PUBLISH_INTERVAL_MS) {
    return false;
  }

  const String telemetryTopic = buildTelemetryTopic_();
  const bool published = mqttClient_.publish(telemetryTopic.c_str(), payload.c_str(), true);
  const bool stillConnected = mqttClient_.connected();

  if (published && stillConnected) {
    lastPublishMs_ = nowMs;
    transportFailure_ = false;
  } else {
    transportFailure_ = true;
    Serial.print("[MQTT] Publish failed rc=");
    Serial.println(mqttClient_.state());
  }

  return published && stillConnected;
}

bool MqttService::publishBacklogPayload(const String& payload) {
  if (!initialized_ || !mqttClient_.connected()) {
    return false;
  }

  const String telemetryTopic = buildTelemetryTopic_();
  const bool published = mqttClient_.publish(telemetryTopic.c_str(), payload.c_str(), true);
  const bool stillConnected = mqttClient_.connected();
  if (published && stillConnected) {
    transportFailure_ = false;
  } else {
    transportFailure_ = true;
    Serial.print("[MQTT] Backlog publish failed rc=");
    Serial.println(mqttClient_.state());
  }
  return published && stillConnected;
}

void MqttService::loop() {
  if (!initialized_) {
    return;
  }

  mqttClient_.loop();
}

bool MqttService::isConfigured() const {
  const char* mqttHost = ConfigService::getMqttHost();
  const char* deviceId = ConfigService::getDeviceId();
  const char* mqttUsername = ConfigService::getMqttUsername();
  const char* mqttPassword = ConfigService::getMqttPassword();
  const uint16_t mqttPort = ConfigService::getMqttPort();
  return mqttHost != nullptr && mqttHost[0] != '\0' && deviceId != nullptr &&
         deviceId[0] != '\0' && mqttUsername != nullptr && mqttUsername[0] != '\0' &&
         mqttPassword != nullptr && mqttPassword[0] != '\0' && mqttPort != 0;
}

bool MqttService::isConnected() {
  return mqttClient_.connected();
}

bool MqttService::hasTransportFailure() const {
  return transportFailure_;
}

bool MqttService::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool MqttService::shouldRequestNetwork(uint32_t nowMs) {
  if (!initialized_ || !isConfigured()) {
    return false;
  }

  if ((nowMs - lastPublishMs_) >= ConfigService::MQTT_PUBLISH_INTERVAL_MS) {
    return true;
  }

  return false;
}

void MqttService::mqttCallback_(char* topic, uint8_t* payload, unsigned int length) {
  if (g_activeMqttService == nullptr) {
    return;
  }

  g_activeMqttService->handleMessage_(topic, payload, length);
}

void MqttService::subscribeTopics_() {
  const String configTopic = buildConfigTopic_();
  const String commandTopic = buildCommandTopic_();
  mqttClient_.subscribe(configTopic.c_str());
  mqttClient_.subscribe(commandTopic.c_str());
}

String MqttService::buildTopicBase_() const {
  String base = String(ConfigService::MQTT_TOPIC_ROOT);
  base += "/";
  base += ConfigService::BRANCH_ID;
  base += "/";
  base += ConfigService::ROOM_ID;
  base += "/";
  base += ConfigService::getDeviceId();
  return base;
}

String MqttService::buildTelemetryTopic_() const {
  return buildTopicBase_() + "/telemetry";
}

String MqttService::buildStatusTopic_() const {
  return buildTopicBase_() + "/status";
}

String MqttService::buildHeartbeatTopic_() const {
  return buildTopicBase_() + "/heartbeat";
}

String MqttService::buildConfigTopic_() const {
  return buildTopicBase_() + "/config";
}

String MqttService::buildCommandTopic_() const {
  return buildTopicBase_() + "/command";
}

void MqttService::handleMessage_(char* topic, uint8_t* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    message += static_cast<char>(payload[i]);
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, message);

  Serial.print("[MQTT] Config topic ");
  Serial.println(topic);

  if (error) {
    Serial.print("[MQTT] Config parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  Serial.print("[MQTT] Config payload ");
  serializeJson(doc, Serial);
  Serial.println();
}

String MqttService::buildPayload_(const TelemetryData& telemetry) const {
  // Payload JSON dibuat terstruktur agar siap untuk backend multi-site.
  JsonDocument doc;
  doc["company"] = ConfigService::COMPANY_ID;
  doc["branch"] = ConfigService::BRANCH_ID;
  doc["room"] = ConfigService::ROOM_ID;
  doc["device"] = ConfigService::getDeviceId();
  doc["timestamp"] = telemetry.timestampIso;
  doc["temperature_c"] = telemetry.temperatureC;
  doc["humidity_pct"] = telemetry.humidityPct;
  doc["battery_percent"] = telemetry.batteryPercent;
  doc["sample_count"] = telemetry.sampleCount;
  doc["sensor_ok"] = telemetry.sensorOk;
  doc["rtc_synced"] = telemetry.rtcSynced;
  doc["ntp_synced"] = telemetry.ntpSynced;

  String payload;
  serializeJson(doc, payload);
  return payload;
}
