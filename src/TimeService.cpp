#include "TimeService.h"

#include <WiFi.h>
#include <time.h>

#include "ConfigService.h"

namespace {

constexpr time_t kMinValidEpoch = 1577836800;  // 2020-01-01T00:00:00Z

}  // namespace

bool TimeService::begin() {
  // RTC dipakai sebagai sumber waktu lokal saat tidak ada internet.
  rtcAvailable_ = rtc_.begin();
  systemTimeSynced_ = false;

  if (!rtcAvailable_) {
    Serial.println("[RTC] DS3231 not found");
    return false;
  }

  const DateTime rtcNow = rtc_.now();
  const bool valid = isDateTimeValid_(rtcNow);
  Serial.println(valid ? "[RTC] RTC ready" : "[RTC] RTC invalid on boot");
  return valid;
}

bool TimeService::syncFromNtpIfNeeded(TelemetryData& telemetry, uint32_t nowMs) {
  // Sinkronisasi NTP dilakukan berkala lalu hasilnya disimpan ke DS3231.
  telemetry.rtcSynced = rtcAvailable_ && isDateTimeValid_(rtc_.now());

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if ((nowMs - lastNtpSyncMs_) < ConfigService::NTP_RESYNC_INTERVAL_MS && telemetry.ntpSynced) {
    return false;
  }

  const bool synced = syncRtcFromNtp_();
  if (synced) {
    lastNtpSyncMs_ = nowMs;
    telemetry.ntpSynced = true;
    systemTimeSynced_ = true;
    telemetry.rtcSynced = rtcAvailable_ && isDateTimeValid_(rtc_.now());
    Serial.print("[RTC] NTP sync ok ts=");
    Serial.println(getIsoTimestamp());
  } else {
    Serial.println("[RTC] NTP sync failed");
  }

  return synced;
}

String TimeService::getDisplayTime() {
  // Waktu pendek untuk OLED agar tetap ringkas.
  const DateTime now = now_();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02d/%02d %02d:%02d:%02d", now.day(), now.month(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

String TimeService::getIsoTimestamp() {
  // Timestamp ISO dipakai untuk payload telemetry.
  const DateTime now = now_();
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d+07:00", now.year(),
           now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(buffer);
}

bool TimeService::shouldRequestNetwork(uint32_t nowMs) const {
  if (systemTimeSynced_ && (nowMs - lastNtpSyncMs_) < ConfigService::NTP_RESYNC_INTERVAL_MS) {
    return false;
  }

  return true;
}

bool TimeService::syncRtcFromNtp_() {
  configTzTime(ConfigService::TZ_INFO, "pool.ntp.org", "time.nist.gov");

  struct tm timeInfo {};
  if (!getLocalTime(&timeInfo, 10000)) {
    return false;
  }

  const time_t epochNow = mktime(&timeInfo);
  if (epochNow < kMinValidEpoch) {
    return false;
  }

  if (rtcAvailable_) {
    rtc_.adjust(DateTime(timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                         timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec));
    Serial.println("[RTC] RTC adjusted from NTP");
  }

  return true;
}

DateTime TimeService::now_() {
  if (rtcAvailable_) {
    const DateTime rtcNow = rtc_.now();
    if (isDateTimeValid_(rtcNow)) {
      return rtcNow;
    }
  }

  if (systemTimeSynced_) {
    time_t epochNow = time(nullptr);
    if (epochNow >= kMinValidEpoch) {
      return DateTime(epochNow);
    }
  }

  // Fallback sederhana bila RTC belum terbaca.
  return DateTime(2000, 1, 1, 0, 0, 0);
}

bool TimeService::isDateTimeValid_(const DateTime& dateTime) const {
  return dateTime.year() >= 2020 && dateTime.year() <= 2099;
}
