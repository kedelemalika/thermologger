#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "AppTypes.h"

// Service MQTT untuk koneksi broker dan publish telemetry periodik.
class MqttService {
 public:
  bool begin();
  void ensureConnection(uint32_t nowMs);
  bool publishTelemetry(const TelemetryData& telemetry, uint32_t nowMs);
  bool publishPayload(const String& payload, uint32_t nowMs);
  bool publishBacklogPayload(const String& payload);
  void loop();
  bool isConfigured() const;
  bool isConnected();
  bool hasTransportFailure() const;
  bool isWifiConnected() const;
  bool shouldRequestNetwork(uint32_t nowMs);

 private:
  String buildCommandTopic_() const;
  static void mqttCallback_(char* topic, uint8_t* payload, unsigned int length);
  String buildConfigTopic_() const;
  String buildHeartbeatTopic_() const;
  String buildPayload_(const TelemetryData& telemetry) const;
  String buildStatusTopic_() const;
  String buildTelemetryTopic_() const;
  String buildTopicBase_() const;
  void handleMessage_(char* topic, uint8_t* payload, unsigned int length);
  void subscribeTopics_();

  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t reconnectBackoffMs_ = 0;
  uint32_t lastPublishMs_ = 0;
  bool initialized_ = false;
  bool transportFailure_ = false;
};
