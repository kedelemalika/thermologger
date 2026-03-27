#include "WiFiService.h"

#include <cstring>
#include <cstdlib>

#include "ConfigService.h"

namespace {

WiFiService* g_activeWifiService = nullptr;

}  // namespace

bool WiFiService::begin() {
  ConfigService::loadRuntimeConfig();
  Serial.println("[WIFI] Service init");

  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);

  wifiManager_.setConfigPortalBlocking(false);
  wifiManager_.setConnectTimeout(ConfigService::WIFI_CONNECT_TIMEOUT_MS / 1000UL);
  wifiManager_.setConfigPortalTimeout(ConfigService::WIFI_PORTAL_TIMEOUT_SEC);
  wifiManager_.setAPCallback(WiFiService::apCallback_);
  wifiManager_.setSaveConfigCallback(WiFiService::saveConfigCallback_);
  wifiManager_.setDebugOutput(false);

  initialized_ = true;
  configSaveRequested_ = false;
  portalActive_ = false;
  connectAttemptStartedMs_ = 0;
  lastReconnectAttemptMs_ = 0;
  reconnectBackoffMs_ = ConfigService::WIFI_INITIAL_BACKOFF_MS;
  failureCount_ = 0;
  networkRequestedUntilMs_ = millis() + ConfigService::WIFI_CONNECT_TIMEOUT_MS;
  lastStateChangeMs_ = millis();
  g_activeWifiService = this;

  if (!paramsAdded_) {
    strncpy(mqttHostBuffer_, ConfigService::getMqttHost(), sizeof(mqttHostBuffer_) - 1);
    mqttHostBuffer_[sizeof(mqttHostBuffer_) - 1] = '\0';

    if (ConfigService::getMqttPort() > 0) {
      snprintf(mqttPortBuffer_, sizeof(mqttPortBuffer_), "%u", ConfigService::getMqttPort());
    } else {
      mqttPortBuffer_[0] = '\0';
    }

    strncpy(deviceIdBuffer_, ConfigService::getDeviceId(), sizeof(deviceIdBuffer_) - 1);
    deviceIdBuffer_[sizeof(deviceIdBuffer_) - 1] = '\0';

    mqttHostParam_ = new WiFiManagerParameter("mqtt_host", "MQTT Broker", mqttHostBuffer_,
                                              sizeof(mqttHostBuffer_));

    mqttPortParam_ = new WiFiManagerParameter("mqtt_port", "MQTT Port", mqttPortBuffer_,
                                              sizeof(mqttPortBuffer_));

    deviceIdParam_ = new WiFiManagerParameter("device_id", "Device ID", deviceIdBuffer_,
                                              sizeof(deviceIdBuffer_));

    wifiManager_.addParameter(mqttHostParam_);
    wifiManager_.addParameter(mqttPortParam_);
    wifiManager_.addParameter(deviceIdParam_);
    paramsAdded_ = true;
  }

  setState_(State::INIT);

  if (shouldStartPortal_()) {
    Serial.println("[WIFI] Provisioning mode requested");
    startConfigPortal_();
  } else if (hasSavedCredentials_()) {
    beginConnectionAttempt_(millis(), false);
  } else {
    Serial.println("[WIFI] No saved credentials");
    setState_(State::FAILED);
  }

  return true;
}

void WiFiService::loop() {
  const uint32_t nowMs = millis();

  if (!initialized_) {
    return;
  }

  if (portalActive_) {
    wifiManager_.process();

    if (!wifiManager_.getConfigPortalActive()) {
      syncCustomParamsToConfig_();
      portalActive_ = false;
      if (WiFi.status() == WL_CONNECTED) {
        handleConnected_(nowMs);
      } else if (hasSavedCredentials_()) {
        beginConnectionAttempt_(nowMs, true);
      } else {
        setState_(State::FAILED);
      }
    }

    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    handleConnected_(nowMs);
    return;
  }

  if (state_ == State::INIT) {
    if (shouldStartPortal_()) {
      startConfigPortal_();
    } else if (hasSavedCredentials_()) {
      beginConnectionAttempt_(nowMs, false);
    } else {
      setState_(State::FAILED);
    }
    return;
  }

  if (state_ == State::CONNECTING || state_ == State::RECONNECTING) {
    if ((nowMs - connectAttemptStartedMs_) >= ConfigService::WIFI_CONNECT_TIMEOUT_MS) {
      handleConnectTimeout_(nowMs);
    }
    return;
  }

  if (state_ == State::FAILED &&
      (nowMs - lastReconnectAttemptMs_) >= reconnectBackoffMs_) {
    if (shouldStartPortal_()) {
      startConfigPortal_();
      return;
    }

    if (!hasSavedCredentials_()) {
      return;
    }

    beginConnectionAttempt_(nowMs, true);
    return;
  }

  if (state_ == State::FAILED) {
    return;
  }

  if (shouldStartPortal_()) {
    startConfigPortal_();
    return;
  }

  handleDisconnected_(nowMs);
}

bool WiFiService::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiService::isPortalActive() const {
  return portalActive_;
}

bool WiFiService::isProvisioningMode() const {
  return ConfigService::getDeviceMode() == ConfigService::DeviceMode::PROVISIONING;
}

void WiFiService::requestNetwork(uint32_t keepAliveMs) {
  const uint32_t nowMs = millis();
  const uint32_t requestedUntil = nowMs + keepAliveMs;

  if (requestedUntil > networkRequestedUntilMs_) {
    networkRequestedUntilMs_ = requestedUntil;
  }

  if (state_ == State::INIT) {
    if (hasSavedCredentials_()) {
      beginConnectionAttempt_(nowMs, false);
    }
    return;
  }

  if (state_ == State::FAILED) {
    if (!hasSavedCredentials_()) {
      return;
    }

    if ((nowMs - lastReconnectAttemptMs_) >= reconnectBackoffMs_) {
      beginConnectionAttempt_(nowMs, true);
    }
  }
}

void WiFiService::releaseNetwork() {
  if (networkRequestedUntilMs_ != 0) {
    Serial.println("[WIFI] Network released");
  }
  networkRequestedUntilMs_ = 0;
}

void WiFiService::resetCredentialsAndRestart() {
  Serial.println("[WIFI] Reset credentials and restart");
  wifiManager_.resetSettings();
  ConfigService::setMqttHost("");
  ConfigService::setMqttPort(0);
  ConfigService::setDeviceId("");
  ConfigService::setDeviceMode(ConfigService::DeviceMode::PROVISIONING);
  ConfigService::saveRuntimeConfig();
  delay(100);
  ESP.restart();
}

void WiFiService::setBatteryModeEnabled(bool enabled) {
  batteryModeEnabled_ = enabled;
}

void WiFiService::setStatusCallback(StatusCallback callback) {
  statusCallback_ = callback;
}

WiFiService::State WiFiService::state() const {
  return state_;
}

const char* WiFiService::stateName() const {
  return stateName_(state_);
}

void WiFiService::apCallback_(WiFiManager* manager) {
  if (g_activeWifiService == nullptr) {
    return;
  }

  g_activeWifiService->portalActive_ = true;
  Serial.print("[WIFI] Portal active SSID=");
  Serial.println(manager->getConfigPortalSSID());
}

void WiFiService::saveConfigCallback_() {
  if (g_activeWifiService != nullptr) {
    g_activeWifiService->configSaveRequested_ = true;
  }

  Serial.println("[WIFI] Credentials/config saved");
}

bool WiFiService::hasSavedCredentials_() {
  return wifiManager_.getWiFiIsSaved();
}

bool WiFiService::isNetworkRequested_(uint32_t nowMs) const {
  return networkRequestedUntilMs_ > nowMs;
}

void WiFiService::beginConnectionAttempt_(uint32_t nowMs, bool reconnecting) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.begin();
  Serial.print("[WIFI] Connect attempt ");
  Serial.println(reconnecting ? "reconnect" : "initial");

  connectAttemptStartedMs_ = nowMs;
  lastReconnectAttemptMs_ = nowMs;
  setState_(reconnecting ? State::RECONNECTING : State::CONNECTING);
}

void WiFiService::disconnectIfIdle_(uint32_t nowMs) {
  if (!batteryModeEnabled_ || portalActive_ || !isConnected()) {
    return;
  }

  if (isNetworkRequested_(nowMs)) {
    return;
  }

  WiFi.disconnect(false, false);
  Serial.println("[WIFI] Idle disconnect");
  setState_(State::INIT);
}

void WiFiService::handleConnected_(uint32_t nowMs) {
  connectAttemptStartedMs_ = 0;
  failureCount_ = 0;
  reconnectBackoffMs_ = ConfigService::WIFI_INITIAL_BACKOFF_MS;

  if (state_ != State::CONNECTED) {
    Serial.print("[WIFI] Connected IP=");
    Serial.println(WiFi.localIP());
    setState_(State::CONNECTED);
  }

  disconnectIfIdle_(nowMs);
}

void WiFiService::handleConnectTimeout_(uint32_t nowMs) {
  WiFi.disconnect(false, false);
  failureCount_++;
  Serial.print("[WIFI] Connect timeout failures=");
  Serial.println(failureCount_);
  setState_(State::FAILED);
  lastReconnectAttemptMs_ = nowMs;

  reconnectBackoffMs_ = min(static_cast<uint32_t>(reconnectBackoffMs_ * 2UL),
                            ConfigService::WIFI_MAX_BACKOFF_MS);
  Serial.print("[WIFI] Retry backoff ms=");
  Serial.println(reconnectBackoffMs_);
}

void WiFiService::handleDisconnected_(uint32_t nowMs) {
  if (portalActive_) {
    return;
  }

  failureCount_++;
  Serial.print("[WIFI] Disconnected failures=");
  Serial.println(failureCount_);
  setState_(State::FAILED);
  lastReconnectAttemptMs_ = nowMs;

  if (shouldStartPortal_()) {
    startConfigPortal_();
    return;
  }

  reconnectBackoffMs_ = min(static_cast<uint32_t>(reconnectBackoffMs_ * 2UL),
                            ConfigService::WIFI_MAX_BACKOFF_MS);
  Serial.print("[WIFI] Retry backoff ms=");
  Serial.println(reconnectBackoffMs_);
}

bool WiFiService::shouldStartPortal_() const {
  return ConfigService::getDeviceMode() == ConfigService::DeviceMode::PROVISIONING;
}

void WiFiService::setState_(State state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  lastStateChangeMs_ = millis();
  Serial.print("[WIFI] State -> ");
  Serial.println(stateName_(state_));

  if (statusCallback_ != nullptr) {
    statusCallback_(state_);
  }
}

const char* WiFiService::stateName_(State state) {
  switch (state) {
    case State::INIT:
      return "INIT";
    case State::CONNECTING:
      return "CONNECTING";
    case State::CONNECTED:
      return "CONNECTED";
    case State::FAILED:
      return "FAILED";
    case State::CONFIG_PORTAL:
      return "CONFIG_PORTAL";
    case State::RECONNECTING:
      return "RECONNECTING";
  }

  return "UNKNOWN";
}

void WiFiService::syncCustomParamsToConfig_() {
  if (mqttHostParam_ == nullptr || mqttPortParam_ == nullptr || deviceIdParam_ == nullptr) {
    return;
  }

  if (!configSaveRequested_) {
    return;
  }

  ConfigService::setMqttHost(mqttHostParam_->getValue());
  ConfigService::setDeviceId(deviceIdParam_->getValue());

  const char* mqttPortValue = mqttPortParam_->getValue();
  const uint32_t mqttPort = strtoul(mqttPortValue, nullptr, 10);
  ConfigService::setMqttPort(static_cast<uint16_t>(mqttPort));
  ConfigService::setDeviceMode(ConfigService::DeviceMode::NORMAL);
  ConfigService::saveRuntimeConfig();
  configSaveRequested_ = false;
  Serial.println("[WIFI] Runtime config stored");
}

void WiFiService::startConfigPortal_() {
  if (portalActive_) {
    return;
  }

  WiFi.disconnect(false, false);
  Serial.println("[WIFI] Starting config portal");
  wifiManager_.startConfigPortal(ConfigService::WIFI_AP_NAME, ConfigService::WIFI_AP_PASSWORD);
  portalActive_ = true;
  connectAttemptStartedMs_ = 0;
  setState_(State::CONFIG_PORTAL);
}
