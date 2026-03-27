#pragma once

#include <Arduino.h>

#include "AppTypes.h"

class MqttService;

// Logger lokal untuk telemetry saat offline, dengan replay bertahap saat online kembali.
class OfflineLoggerService {
 public:
  bool begin();
  bool append(const TelemetryData& telemetry);
  uint8_t flushPending(MqttService& mqttService, uint32_t nowMs);
  bool hasBacklog() const;
  size_t getPendingCount() const;
  bool shouldRequestNetwork(uint32_t nowMs) const;

 private:
  bool dropOldestRecord_();
  String buildPayload_(const TelemetryData& telemetry) const;
  bool isDuplicateOfLastRecord_(const String& payload) const;
  bool recoverFiles_();
  size_t countRecords_() const;
  size_t fileSize_() const;

  uint32_t lastFlushAttemptMs_ = 0;
  bool initialized_ = false;
};
