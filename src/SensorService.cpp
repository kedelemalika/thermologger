#include "SensorService.h"

#include <Wire.h>

#include "ConfigService.h"

namespace {

constexpr uint8_t kMaxReadAttempts = 3;  // 1 initial try + 2 retries

bool isReadingValid(float temperatureC, float humidityPct) {
  if (isnan(temperatureC) || isnan(humidityPct)) {
    return false;
  }

  // Range aman berdasarkan karakteristik umum SHT20.
  if (temperatureC < -40.0f || temperatureC > 125.0f) {
    return false;
  }

  if (humidityPct < 0.0f || humidityPct > 100.0f) {
    return false;
  }

  return true;
}

}  // namespace

bool SensorService::begin() {
  // Inisialisasi sensor setelah bus I2C aktif.
  if (!sht20_.begin()) {
    initialized_ = false;
    Serial.println("[SENSOR] Init failed");
    return false;
  }

  const SensorReading initialReading = read();
  initialized_ = initialReading.valid;
  Serial.println(initialReading.valid ? "[SENSOR] Init ok" : "[SENSOR] Init read invalid");
  return initialized_;
}

SensorService::SensorReading SensorService::read() {
  SensorReading reading {NAN, NAN, false};

  for (uint8_t attempt = 0; attempt < kMaxReadAttempts; ++attempt) {
    if (!sht20_.read()) {
      if (attempt + 1 < kMaxReadAttempts) {
        delay(10);
      }
      continue;
    }

    const float temperature = sht20_.getTemperature();
    const float humidity = sht20_.getHumidity();

    if (isReadingValid(temperature, humidity)) {
      reading.temperatureC = temperature;
      reading.humidityPct = humidity;
      reading.valid = true;
      return reading;
    }

    if (attempt + 1 < kMaxReadAttempts) {
      delay(10);
    }
  }

  return reading;
}

bool SensorService::update(TelemetryData& telemetry, uint32_t nowMs) {
  if (!initialized_) {
    telemetry.sensorOk = false;
    return false;
  }

  // Baca sensor hanya saat interval tercapai untuk hemat daya.
  if ((nowMs - lastReadMs_) < ConfigService::SENSOR_INTERVAL_MS && telemetry.sampleCount > 0) {
    return false;
  }

  const SensorReading reading = read();
  telemetry.temperatureC = reading.temperatureC;
  telemetry.humidityPct = reading.humidityPct;
  telemetry.sensorOk = reading.valid;

  if (telemetry.sensorOk) {
    telemetry.sampleCount++;
  } else {
    Serial.println("[SENSOR] Read invalid");
  }

  lastReadMs_ = nowMs;
  return true;
}

uint32_t SensorService::lastReadAt() const {
  return lastReadMs_;
}
