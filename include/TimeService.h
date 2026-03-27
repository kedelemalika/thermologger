#pragma once

#include <Arduino.h>
#include <RTClib.h>

#include "AppTypes.h"

// Service waktu untuk sinkron NTP, update DS3231, dan format timestamp.
class TimeService {
 public:
  bool begin();
  bool syncFromNtpIfNeeded(TelemetryData& telemetry, uint32_t nowMs);
  String getDisplayTime();
  String getIsoTimestamp();
  bool shouldRequestNetwork(uint32_t nowMs) const;

 private:
  bool isDateTimeValid_(const DateTime& dateTime) const;
  bool syncRtcFromNtp_();
  DateTime now_();

  RTC_DS3231 rtc_;
  uint32_t lastNtpSyncMs_ = 0;
  bool rtcAvailable_ = false;
  bool systemTimeSynced_ = false;
};
