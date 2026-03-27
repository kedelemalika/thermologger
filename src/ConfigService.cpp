#include "ConfigService.h"

#include <cstring>

#include <Preferences.h>

namespace {

constexpr char kPrefsNamespace[] = "appcfg";
constexpr char kPrefsMqttHost[] = "mqtt_host";
constexpr char kPrefsMqttPort[] = "mqtt_port";
constexpr char kPrefsDeviceId[] = "device_id";
constexpr char kPrefsDeviceMode[] = "dev_mode";

char g_mqttHost[ConfigService::MQTT_HOST_MAX_LEN] = "";
char g_deviceId[ConfigService::DEVICE_ID_MAX_LEN] = "";
uint16_t g_mqttPort = 0;
ConfigService::DeviceMode g_deviceMode = ConfigService::DeviceMode::PROVISIONING;

void copyStringValue(char* destination, size_t destinationSize, const char* source) {
  if (destinationSize == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

}  // namespace

bool ConfigService::loadRuntimeConfig() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    return false;
  }

  const String mqttHost = prefs.getString(kPrefsMqttHost, "");
  const String deviceId = prefs.getString(kPrefsDeviceId, "");
  g_mqttPort = prefs.getUShort(kPrefsMqttPort, 0);
  g_deviceMode = static_cast<ConfigService::DeviceMode>(
      prefs.getUChar(kPrefsDeviceMode, static_cast<uint8_t>(ConfigService::DeviceMode::PROVISIONING)));

  copyStringValue(g_mqttHost, sizeof(g_mqttHost), mqttHost.c_str());
  copyStringValue(g_deviceId, sizeof(g_deviceId), deviceId.c_str());

  prefs.end();
  return true;
}

bool ConfigService::saveRuntimeConfig() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }

  prefs.putString(kPrefsMqttHost, g_mqttHost);
  prefs.putUShort(kPrefsMqttPort, g_mqttPort);
  prefs.putString(kPrefsDeviceId, g_deviceId);
  prefs.putUChar(kPrefsDeviceMode, static_cast<uint8_t>(g_deviceMode));
  prefs.end();
  return true;
}

const char* ConfigService::getMqttHost() {
  return g_mqttHost;
}

uint16_t ConfigService::getMqttPort() {
  return g_mqttPort;
}

const char* ConfigService::getDeviceId() {
  return g_deviceId;
}

ConfigService::DeviceMode ConfigService::getDeviceMode() {
  return g_deviceMode;
}

void ConfigService::setMqttHost(const char* mqttHost) {
  copyStringValue(g_mqttHost, sizeof(g_mqttHost), mqttHost);
}

void ConfigService::setMqttPort(uint16_t mqttPort) {
  g_mqttPort = mqttPort;
}

void ConfigService::setDeviceId(const char* deviceId) {
  copyStringValue(g_deviceId, sizeof(g_deviceId), deviceId);
}

void ConfigService::setDeviceMode(DeviceMode deviceMode) {
  g_deviceMode = deviceMode;
}
