#include "OfflineLoggerService.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "ConfigService.h"
#include "MqttService.h"

namespace {

constexpr char kLogPath[] = "/telemetry.log";
constexpr char kTempPath[] = "/telemetry.tmp";
constexpr uint8_t kFlushBatchSize = 5;

}  // namespace

bool OfflineLoggerService::begin() {
  initialized_ = LittleFS.begin(true);
  if (initialized_) {
    recoverFiles_();
    Serial.print("[LOGGER] Ready pending=");
    Serial.println(getPendingCount());
  } else {
    Serial.println("[LOGGER] LittleFS init failed");
  }
  return initialized_;
}

bool OfflineLoggerService::append(const TelemetryData& telemetry) {
  if (!initialized_) {
    return false;
  }

  const String payload = buildPayload_(telemetry);
  if (payload.length() == 0 || isDuplicateOfLastRecord_(payload)) {
    return false;
  }

  while ((countRecords_() >= ConfigService::OFFLINE_LOG_MAX_RECORDS) ||
         (fileSize_() >= ConfigService::OFFLINE_LOG_MAX_BYTES)) {
    if (!dropOldestRecord_()) {
      break;
    }
  }

  File file = LittleFS.open(kLogPath, FILE_APPEND);
  if (!file) {
    return false;
  }

  const bool written = file.println(payload) > 0;
  file.close();
  if (!written) {
    Serial.println("[LOGGER] Append failed");
  }
  return written;
}

uint8_t OfflineLoggerService::flushPending(MqttService& mqttService, uint32_t nowMs) {
  if (!initialized_ || !mqttService.isConnected() || !hasBacklog()) {
    return 0;
  }

  if ((nowMs - lastFlushAttemptMs_) < ConfigService::OFFLINE_FLUSH_INTERVAL_MS) {
    return 0;
  }

  File source = LittleFS.open(kLogPath, FILE_READ);
  if (!source) {
    return 0;
  }

  File temp = LittleFS.open(kTempPath, FILE_WRITE);
  if (!temp) {
    source.close();
    lastFlushAttemptMs_ = nowMs;
    return 0;
  }

  uint8_t sentCount = 0;
  while (source.available()) {
    const String line = source.readStringUntil('\n');
    if (line.length() == 0) {
      continue;
    }

    if (sentCount < kFlushBatchSize) {
      if (mqttService.publishBacklogPayload(line)) {
        ++sentCount;
        continue;
      }
    }

    temp.println(line);
  }

  source.close();
  const size_t remainingBytes = temp.size();
  temp.close();
  LittleFS.remove(kLogPath);
  if (remainingBytes > 0) {
    LittleFS.rename(kTempPath, kLogPath);
  } else {
    LittleFS.remove(kTempPath);
  }
  lastFlushAttemptMs_ = nowMs;
  return sentCount;
}

bool OfflineLoggerService::hasBacklog() const {
  if (!initialized_) {
    return false;
  }

  return LittleFS.exists(kLogPath) && fileSize_() > 0;
}

size_t OfflineLoggerService::getPendingCount() const {
  if (!initialized_) {
    return 0;
  }

  return countRecords_();
}

bool OfflineLoggerService::shouldRequestNetwork(uint32_t nowMs) const {
  return hasBacklog() && (nowMs - lastFlushAttemptMs_) >= ConfigService::OFFLINE_FLUSH_INTERVAL_MS;
}

bool OfflineLoggerService::dropOldestRecord_() {
  File source = LittleFS.open(kLogPath, FILE_READ);
  if (!source) {
    return false;
  }

  source.readStringUntil('\n');

  File temp = LittleFS.open(kTempPath, FILE_WRITE);
  if (!temp) {
    source.close();
    return false;
  }

  while (source.available()) {
    const String line = source.readStringUntil('\n');
    if (line.length() == 0) {
      continue;
    }
    temp.println(line);
  }

  source.close();
  temp.close();
  LittleFS.remove(kLogPath);
  LittleFS.rename(kTempPath, kLogPath);
  Serial.println("[LOGGER] Drop oldest record");
  return true;
}

String OfflineLoggerService::buildPayload_(const TelemetryData& telemetry) const {
  JsonDocument doc;
  doc["company"] = ConfigService::COMPANY_ID;
  doc["branch"] = ConfigService::BRANCH_ID;
  doc["room"] = ConfigService::ROOM_ID;
  doc["device"] = ConfigService::getDeviceId();
  doc["timestamp"] = telemetry.timestampIso;
  doc["temperature_c"] = telemetry.temperatureC;
  doc["humidity_pct"] = telemetry.humidityPct;
  doc["battery_percent"] = telemetry.batteryPercent;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

bool OfflineLoggerService::isDuplicateOfLastRecord_(const String& payload) const {
  File file = LittleFS.open(kLogPath, FILE_READ);
  if (!file) {
    return false;
  }

  String lastLine;
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      lastLine = line;
    }
  }

  file.close();
  return lastLine == payload;
}

bool OfflineLoggerService::recoverFiles_() {
  const bool hasLog = LittleFS.exists(kLogPath);
  const bool hasTemp = LittleFS.exists(kTempPath);

  if (!hasTemp) {
    return true;
  }

  // Jika restart terjadi saat rotate file, utamakan file log utama yang utuh.
  if (hasLog) {
    LittleFS.remove(kTempPath);
    Serial.println("[LOGGER] Recovery dropped temp file");
    return true;
  }

  const bool recovered = LittleFS.rename(kTempPath, kLogPath);
  if (recovered) {
    Serial.println("[LOGGER] Recovery restored temp file");
  }
  return recovered;
}

size_t OfflineLoggerService::countRecords_() const {
  File file = LittleFS.open(kLogPath, FILE_READ);
  if (!file) {
    return 0;
  }

  size_t count = 0;
  while (file.available()) {
    const String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      ++count;
    }
  }
  file.close();
  return count;
}

size_t OfflineLoggerService::fileSize_() const {
  File file = LittleFS.open(kLogPath, FILE_READ);
  if (!file) {
    return 0;
  }

  const size_t size = file.size();
  file.close();
  return size;
}
