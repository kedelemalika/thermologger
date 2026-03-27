#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Pusat konfigurasi supaya pin, interval, dan topic tidak tersebar.
class ConfigService {
 public:
  enum class DeviceMode : uint8_t {
    PROVISIONING = 0,
    NORMAL = 1,
  };

  static constexpr size_t MQTT_HOST_MAX_LEN = 64;
  static constexpr size_t DEVICE_ID_MAX_LEN = 32;

  static constexpr uint8_t I2C_SDA_PIN = 21;
  static constexpr uint8_t I2C_SCL_PIN = 22;
  static constexpr uint8_t OLED_RESET_PIN = 255;
  static constexpr uint8_t OLED_WIDTH = 128;
  static constexpr uint8_t OLED_HEIGHT = 64;
  static constexpr uint8_t BUTTON_PIN = 27;

  static constexpr uint32_t SENSOR_INTERVAL_MS = 30UL * 1000UL;
  static constexpr uint32_t DISPLAY_TIMEOUT_MS = 15UL * 1000UL;
  static constexpr uint32_t WIFI_RESET_HOLD_MS = 5UL * 1000UL;
  static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 60UL * 1000UL;
  static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 10UL * 1000UL;
  static constexpr uint32_t OFFLINE_FLUSH_INTERVAL_MS = 5UL * 1000UL;
  static constexpr uint32_t NTP_RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
  static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15UL * 1000UL;
  static constexpr uint32_t WIFI_INITIAL_BACKOFF_MS = 5UL * 1000UL;
  static constexpr uint32_t WIFI_MAX_BACKOFF_MS = 60UL * 1000UL;
  static constexpr uint32_t WIFI_IDLE_DISCONNECT_MS = 3UL * 1000UL;
  static constexpr uint16_t WIFI_PORTAL_TIMEOUT_SEC = 180;
  static constexpr uint8_t WIFI_FAILURES_BEFORE_PORTAL = 3;
  static constexpr size_t OFFLINE_LOG_MAX_RECORDS = 128;
  static constexpr size_t OFFLINE_LOG_MAX_BYTES = 32UL * 1024UL;

  static constexpr const char* WIFI_AP_NAME = "esp32-room-setup";
  static constexpr const char* WIFI_AP_PASSWORD = "setup1234";

  static constexpr const char* MQTT_DEFAULT_HOST = "mqtt.ljkwarehouse.com";
  static constexpr uint16_t MQTT_PORT_TCP = 1883;
  static constexpr uint16_t MQTT_PORT_TLS = 8883;
  static constexpr bool MQTT_USE_TLS = false;

#ifdef APP_MQTT_USERNAME
  static constexpr const char* MQTT_USERNAME = APP_MQTT_USERNAME;
#else
  static constexpr const char* MQTT_USERNAME = "";
#endif

#ifdef APP_MQTT_PASSWORD
  static constexpr const char* MQTT_PASSWORD = APP_MQTT_PASSWORD;
#else
  static constexpr const char* MQTT_PASSWORD = "";
#endif

  static constexpr const char* MQTT_TOPIC_TELEMETRY =
      "company/branch-01/room-01/telemetry";
  static constexpr const char* MQTT_TOPIC_STATUS = "company/branch-01/room-01/status";

  static constexpr const char* ROOM_ID = "room-01";
  static constexpr const char* BRANCH_ID = "branch-01";
  static constexpr const char* COMPANY_ID = "company";
  static constexpr const char* MQTT_TOPIC_ROOT = "site";
  static constexpr const char* TZ_INFO = "WIB-7";

  static bool loadRuntimeConfig();
  static bool saveRuntimeConfig();

  static const char* getMqttHost();
  static uint16_t getMqttPort();
  static const char* getMqttUsername();
  static const char* getMqttPassword();
  static const char* getDeviceId();
  static DeviceMode getDeviceMode();

  static void setMqttHost(const char* mqttHost);
  static void setMqttPort(uint16_t mqttPort);
  static void setDeviceId(const char* deviceId);
  static void setDeviceMode(DeviceMode deviceMode);
};
