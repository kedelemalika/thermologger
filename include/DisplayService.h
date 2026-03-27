#pragma once

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "AppTypes.h"

// Service OLED yang default mati dan hanya aktif saat diminta.
class DisplayService {
 public:
  DisplayService();
  bool begin();
  void setEnabled(bool enabled);
  bool isEnabled() const;
  void render(const TelemetryData& telemetry, const String& timeText);

 private:
  void renderHeader_(const String& timeText);
  void renderBody_(const TelemetryData& telemetry);

  Adafruit_SSD1306 display_;
  bool enabled_ = false;
  bool initialized_ = false;
};
