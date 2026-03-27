#pragma once

#include <Arduino.h>
#include <SHT2x.h>

#include "AppTypes.h"

// Service pembacaan sensor SHT20 dengan interval non-blocking.
class SensorService {
 public:
  struct SensorReading {
    float temperatureC;
    float humidityPct;
    bool valid;
  };

  bool begin();
  bool update(TelemetryData& telemetry, uint32_t nowMs);
  SensorReading read();
  uint32_t lastReadAt() const;

 private:
  SHT20 sht20_;
  uint32_t lastReadMs_ = 0;
  bool initialized_ = false;
};
