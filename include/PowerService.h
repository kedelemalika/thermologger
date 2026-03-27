#pragma once

#include <Arduino.h>

// Service daya untuk tombol OLED, timeout layar, dan idle ringan.
class PowerService {
 public:
  bool begin();
  void enableBatteryMode(bool enabled, uint64_t deepSleepPeriodMs = 0);
  void enterDeepSleepNow() const;
  bool isBatteryModeEnabled() const;
  bool shouldEnterDeepSleep(uint32_t nowMs) const;
  bool shouldResetWifi(uint32_t nowMs);
  bool shouldWakeDisplay(uint32_t nowMs);
  bool shouldTurnOffDisplay(uint32_t nowMs) const;
  void notifyDisplayEnabled(uint32_t nowMs);
  void notifyDisplayDisabled();
  void idle();

 private:
  bool lastButtonState_ = HIGH;
  bool buttonPressedLatched_ = false;
  bool wifiResetLatched_ = false;
  uint32_t lastDebounceMs_ = 0;
  uint32_t buttonPressedAtMs_ = 0;
  uint32_t displayEnabledAtMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  bool displayActive_ = false;
  bool batteryModeEnabled_ = false;
  uint64_t deepSleepPeriodUs_ = 0;
};
