#include "DisplayService.h"

#include <Wire.h>

#include "ConfigService.h"

namespace {

constexpr uint8_t kOledI2cAddress = 0x3C;

}  // namespace

DisplayService::DisplayService()
    : display_(ConfigService::OLED_WIDTH, ConfigService::OLED_HEIGHT, &Wire,
               ConfigService::OLED_RESET_PIN) {}

bool DisplayService::begin() {
  // OLED dinyalakan sekali untuk inisialisasi lalu langsung dimatikan.
  if (!display_.begin(SSD1306_SWITCHCAPVCC, kOledI2cAddress)) {
    Serial.println("[DISPLAY] Init failed");
    return false;
  }

  initialized_ = true;
  display_.setTextWrap(false);
  display_.setTextColor(SSD1306_WHITE);
  display_.clearDisplay();
  display_.display();
  setEnabled(false);
  Serial.println("[DISPLAY] Ready");
  return true;
}

void DisplayService::setEnabled(bool enabled) {
  if (!initialized_) {
    enabled_ = false;
    return;
  }

  if (enabled_ == enabled) {
    return;
  }

  enabled_ = enabled;
  if (enabled_) {
    display_.ssd1306_command(SSD1306_DISPLAYON);
    Serial.println("[DISPLAY] ON");
  } else {
    display_.clearDisplay();
    display_.display();
    display_.ssd1306_command(SSD1306_DISPLAYOFF);
    Serial.println("[DISPLAY] OFF");
  }
}

bool DisplayService::isEnabled() const {
  return enabled_;
}

void DisplayService::render(const TelemetryData& telemetry, const String& timeText) {
  if (!initialized_ || !enabled_) {
    return;
  }

  display_.clearDisplay();
  renderHeader_(timeText);
  renderBody_(telemetry);
  display_.display();
}

void DisplayService::renderHeader_(const String& timeText) {
  // Jam ditampilkan ringkas pada header agar mudah dibaca.
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print("Jam ");
  display_.println(timeText);
  display_.drawLine(0, 12, ConfigService::OLED_WIDTH - 1, 12, SSD1306_WHITE);
}

void DisplayService::renderBody_(const TelemetryData& telemetry) {
  // Isi utama fokus pada suhu dan kelembaban agar pas di OLED 128x64.
  display_.setTextSize(1);
  display_.setCursor(0, 18);
  display_.print("Suhu");

  display_.setTextSize(2);
  display_.setCursor(0, 28);
  if (telemetry.sensorOk) {
    display_.printf("%.1f C", telemetry.temperatureC);
  } else {
    display_.print("--.- C");
  }

  display_.setTextSize(1);
  display_.setCursor(0, 52);
  display_.print("Hum ");
  if (telemetry.sensorOk) {
    display_.printf("%.1f %%", telemetry.humidityPct);
  } else {
    display_.print("--.- %");
  }
}
