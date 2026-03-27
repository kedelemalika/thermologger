#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "ConfigService.h"

// Service WiFi berbasis WiFiManager untuk auto connect dan captive portal.
class WiFiService {
 public:
  enum class State : uint8_t {
    INIT,
    CONNECTING,
    CONNECTED,
    FAILED,
    CONFIG_PORTAL,
    RECONNECTING,
  };

  using StatusCallback = void (*)(State state);

  bool begin();
  void loop();
  bool isConnected() const;
  bool isPortalActive() const;
  bool isProvisioningMode() const;
  void requestNetwork(uint32_t keepAliveMs = ConfigService::WIFI_IDLE_DISCONNECT_MS);
  void releaseNetwork();
  void resetCredentialsAndRestart();
  void setBatteryModeEnabled(bool enabled);
  void setStatusCallback(StatusCallback callback);
  State state() const;
  const char* stateName() const;

 private:
  static void apCallback_(WiFiManager* manager);
  static void saveConfigCallback_();
  bool hasSavedCredentials_();
  bool isNetworkRequested_(uint32_t nowMs) const;
  void beginConnectionAttempt_(uint32_t nowMs, bool reconnecting);
  void disconnectIfIdle_(uint32_t nowMs);
  void handleConnected_(uint32_t nowMs);
  void handleConnectTimeout_(uint32_t nowMs);
  void handleDisconnected_(uint32_t nowMs);
  bool shouldStartPortal_() const;
  void setState_(State state);
  static const char* stateName_(State state);
  void syncCustomParamsToConfig_();
  void startConfigPortal_();

  WiFiManager wifiManager_;
  WiFiManagerParameter* mqttHostParam_ = nullptr;
  WiFiManagerParameter* mqttPortParam_ = nullptr;
  WiFiManagerParameter* deviceIdParam_ = nullptr;
  char mqttHostBuffer_[ConfigService::MQTT_HOST_MAX_LEN] = {};
  char mqttPortBuffer_[8] = {};
  char deviceIdBuffer_[ConfigService::DEVICE_ID_MAX_LEN] = {};
  uint32_t connectAttemptStartedMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t networkRequestedUntilMs_ = 0;
  uint32_t reconnectBackoffMs_ = ConfigService::WIFI_INITIAL_BACKOFF_MS;
  uint32_t lastStateChangeMs_ = 0;
  uint8_t failureCount_ = 0;
  bool initialized_ = false;
  bool configSaveRequested_ = false;
  bool paramsAdded_ = false;
  bool portalActive_ = false;
  bool batteryModeEnabled_ = false;
  StatusCallback statusCallback_ = nullptr;
  State state_ = State::INIT;
};
