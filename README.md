# Thermologger

**Thermologger** is an ESP32-based IoT temperature logging system designed with a modular architecture. It supports real-time monitoring via MQTT and ensures data reliability with offline logging capabilities.

---

## 🚀 Features

- 📡 **WiFi Connectivity**
- 🌡️ **Temperature Monitoring**
- ☁️ **MQTT Integration**
- 💾 **Offline Data Logging (Failsafe)**
- ⚙️ **Modular Service Architecture**
- 🔋 **Power Management Support**
- 🖥️ **Display Integration (optional)**

---

## 🧠 Architecture Overview

This project follows a **modular service-based architecture**, making it scalable and easy to maintain.

### Core Services:

- `WiFiService` → Handles network connection
- `MqttService` → Manages MQTT communication
- `SensorService` → Reads temperature data
- `OfflineLoggerService` → Stores data when offline
- `DisplayService` → Displays information (if available)
- `PowerService` → Manages power usage
- `TimeService` → Handles time synchronization
- `ConfigService` → Configuration management
- `DeviceController` → Coordinates all services

---

## 📂 Project Structure

```
thermologger/
├── include/
│   ├── *.h
├── src/
│   ├── *.cpp
│   └── main.cpp
├── platformio.ini
└── README.md
```

---

## ⚙️ Requirements

- ESP32 board
- PlatformIO (recommended) or Arduino IDE
- MQTT Broker (e.g., Mosquitto, HiveMQ, etc.)

---

## 🔧 Installation

1. Clone this repository:

   ```bash
   git clone https://github.com/kedelemalika/thermologger.git
   ```

2. Open with PlatformIO:
   - VSCode → Open Folder → select project

3. Configure your settings:
   - WiFi credentials
   - MQTT broker details

4. Build & upload:

   ```bash
   pio run --target upload
   ```

---

## 📡 MQTT Example Topics

```
thermologger/temperature
thermologger/status
```

---

## 🛠️ Future Improvements

- Web dashboard integration
- OTA (Over-The-Air updates)
- Multi-sensor support
- Data visualization

---

## 🤝 Contributing

Contributions are welcome!
Feel free to open issues or submit pull requests.

---

## 📄 License

This project is open-source and available under the MIT License.

---

## 👤 Author

Developed by **Ruslan**

---
