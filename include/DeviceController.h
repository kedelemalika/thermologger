#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "DisplayService.h"
#include "MqttService.h"
#include "OfflineLoggerService.h"
#include "PowerService.h"
#include "SensorService.h"
#include "TimeService.h"
#include "WiFiService.h"

// Orchestrator utama device agar flow setup/loop tidak tersebar di main.cpp.
class DeviceController {
 public:
  enum class State : uint8_t {
    BOOT,
    CONNECTING_WIFI,
    NORMAL_OPERATION,
    DEGRADED_MODE,
    OFFLINE_MODE,
    PROVISIONING_MODE,
    LOW_POWER_MODE,
  };

  DeviceController(SensorService& sensorService, DisplayService& displayService,
                   TimeService& timeService, MqttService& mqttService,
                   WiFiService& wifiService, OfflineLoggerService& offlineLoggerService,
                   PowerService& powerService, TelemetryData& telemetry);

  bool begin();
  void loop();
  State state() const;
  const char* stateName() const;

 private:
  void handleDisplay_(uint32_t nowMs);
  void handleNetworkDemand_(uint32_t nowMs);
  void handleSensorAndTime_(uint32_t nowMs);
  void handleTelemetryPublish_(uint32_t nowMs);
  void refreshState_(uint32_t nowMs);
  void setState_(State state);
  static const char* stateName_(State state);

  SensorService& sensorService_;
  DisplayService& displayService_;
  TimeService& timeService_;
  MqttService& mqttService_;
  WiFiService& wifiService_;
  OfflineLoggerService& offlineLoggerService_;
  PowerService& powerService_;
  TelemetryData& telemetry_;
  uint32_t lastDisplayRenderMs_ = 0;
  uint32_t lastOfflineQueueMs_ = 0;
  String lastOfflineQueuedTimestamp_;
  bool initialized_ = false;
  bool needNtpSync_ = false;
  bool needMqtt_ = false;
  bool needOfflineFlush_ = false;
  bool networkDemandActive_ = false;
  State state_ = State::BOOT;
};
