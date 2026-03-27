#include "PowerService.h"

#include <esp_sleep.h>

#include "ConfigService.h"

namespace {

constexpr uint32_t kButtonDebounceMs = 40;
constexpr uint32_t kIdleBeforeSleepMs = 5000;

}  // namespace

bool PowerService::begin() {
  // Tombol diasumsikan active-low dengan internal pull-up.
  pinMode(ConfigService::BUTTON_PIN, INPUT_PULLUP);
  displayActive_ = false;
  buttonPressedLatched_ = false;
  wifiResetLatched_ = false;
  buttonPressedAtMs_ = 0;
  lastButtonState_ = digitalRead(ConfigService::BUTTON_PIN);
  lastDebounceMs_ = millis();
  lastActivityMs_ = millis();
  Serial.println("[POWER] Service ready");
  return true;
}

void PowerService::enableBatteryMode(bool enabled, uint64_t deepSleepPeriodMs) {
  batteryModeEnabled_ = enabled;
  deepSleepPeriodUs_ = deepSleepPeriodMs * 1000ULL;
  Serial.print("[POWER] Battery mode ");
  Serial.println(enabled ? "ON" : "OFF");
}

bool PowerService::isBatteryModeEnabled() const {
  return batteryModeEnabled_;
}

bool PowerService::shouldResetWifi(uint32_t nowMs) {
  const bool buttonState = digitalRead(ConfigService::BUTTON_PIN);
  if (buttonState != lastButtonState_) {
    lastDebounceMs_ = nowMs;
    lastButtonState_ = buttonState;
  }

  if ((nowMs - lastDebounceMs_) < kButtonDebounceMs) {
    return false;
  }

  if (buttonState == LOW) {
    if (buttonPressedAtMs_ == 0) {
      buttonPressedAtMs_ = nowMs;
    }

    if (!wifiResetLatched_ && (nowMs - buttonPressedAtMs_) >= ConfigService::WIFI_RESET_HOLD_MS) {
      wifiResetLatched_ = true;
      lastActivityMs_ = nowMs;
      Serial.println("[POWER] WiFi reset requested");
      return true;
    }

    return false;
  }

  buttonPressedAtMs_ = 0;
  wifiResetLatched_ = false;
  return false;
}

bool PowerService::shouldWakeDisplay(uint32_t nowMs) {
  // Debounce edge-triggered agar satu tekan hanya menghasilkan satu wake event.
  const bool buttonState = digitalRead(ConfigService::BUTTON_PIN);
  if (buttonState != lastButtonState_) {
    lastDebounceMs_ = nowMs;
    lastButtonState_ = buttonState;
  }

  if ((nowMs - lastDebounceMs_) < kButtonDebounceMs) {
    return false;
  }

  if (buttonState == LOW && !buttonPressedLatched_) {
    if (buttonPressedAtMs_ == 0) {
      buttonPressedAtMs_ = nowMs;
    }

    buttonPressedLatched_ = true;
    lastActivityMs_ = nowMs;
    return true;
  }

  if (buttonState == HIGH) {
    buttonPressedLatched_ = false;
    buttonPressedAtMs_ = 0;
  }

  return false;
}

bool PowerService::shouldTurnOffDisplay(uint32_t nowMs) const {
  // OLED dimatikan otomatis setelah timeout habis.
  return displayActive_ && ((nowMs - displayEnabledAtMs_) >= ConfigService::DISPLAY_TIMEOUT_MS);
}

void PowerService::notifyDisplayEnabled(uint32_t nowMs) {
  displayActive_ = true;
  displayEnabledAtMs_ = nowMs;
  lastActivityMs_ = nowMs;
}

void PowerService::notifyDisplayDisabled() {
  displayActive_ = false;
  lastActivityMs_ = millis();
}

bool PowerService::shouldEnterDeepSleep(uint32_t nowMs) const {
  if (!batteryModeEnabled_ || displayActive_ || deepSleepPeriodUs_ == 0) {
    return false;
  }

  return (nowMs - lastActivityMs_) >= kIdleBeforeSleepMs;
}

void PowerService::enterDeepSleepNow() const {
  if (!batteryModeEnabled_ || deepSleepPeriodUs_ == 0) {
    return;
  }

  Serial.println("[POWER] Enter deep sleep");
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(ConfigService::BUTTON_PIN), 0);
  esp_sleep_enable_timer_wakeup(deepSleepPeriodUs_);
  esp_deep_sleep_start();
}

void PowerService::idle() {
  // Hook battery mode dibuat non-blocking agar loop utama tetap responsif.
  if (shouldEnterDeepSleep(millis())) {
    enterDeepSleepNow();
    return;
  }

  yield();
}
