#pragma once

#include <Arduino.h>

// Struktur data bersama agar tiap service bertukar data dengan konsisten.
struct TelemetryData {
  float temperatureC = NAN;
  float humidityPct = NAN;
  float batteryPercent = NAN;
  String timestampIso;
  uint32_t sampleCount = 0;
  bool rtcSynced = false;
  bool ntpSynced = false;
  bool sensorOk = false;
};
