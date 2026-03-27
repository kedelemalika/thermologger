#include <Arduino.h>
#include <Wire.h>

#include "ConfigService.h"
#include "DeviceController.h"
#include "DisplayService.h"
#include "MqttService.h"
#include "OfflineLoggerService.h"
#include "PowerService.h"
#include "SensorService.h"
#include "TimeService.h"
#include "WiFiService.h"

SensorService sensorService;
DisplayService displayService;
TimeService timeService;
MqttService mqttService;
OfflineLoggerService offlineLoggerService;
PowerService powerService;
WiFiService wifiService;
TelemetryData telemetry;
DeviceController deviceController(sensorService, displayService, timeService, mqttService,
                                  wifiService, offlineLoggerService, powerService, telemetry);

void setup() {
  Serial.begin(115200);
  Wire.begin(ConfigService::I2C_SDA_PIN, ConfigService::I2C_SCL_PIN);
  deviceController.begin();
}

void loop() {
  deviceController.loop();
}
