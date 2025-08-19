# Dual-Tenant Energy Monitoring System

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-PlatformIO-orange.svg)]
[![Language](https://img.shields.io/badge/language-C/C++-green.svg)]

## 📌 Overview

The **Duo Energy Monitoring System** is an **ESP32-based** designed to measure, monitor, and analyze the energy consumption of connected electrical devices in **real-time**.  
It integrates **PZEM-004T energy meters**, **alert indicators**, **SMS Alert**, and **cloud connectivity** for intelligent data visualization and alerts.

This project aims to provide a **low-cost,transparent, modular, and IoT-ready solution** for households, laboratories, and small-scale industries to track power usage, optimize consumption, and enhance energy efficiency.

---

## 🎯 Features

- **Real-time Energy Monitoring** via PZEM-004T modules (voltage, current, power, energy).
- **ESP32-DevKit Control** running **C++** firmware.
- **LED & Buzzer Alerts** for configurable threshold violations.
- **Modular Code Structure** for easy customization.
- **Cloud Integration** (Wi-Fi, GSM-ready) for remote monitoring.
- **Multi-device Support** via Modbus RTU over RS485.
- **Low Power Consumption** design with sleep mode support.

---

## 📦 Hardware Components

| Component                   | Description                                       | Qty |
|-----------------------------|---------------------------------------------------|-----|
| **ESP32**                   | Main MCU with Wi-Fi & Bluetooth                   | 1   |
| **PZEM-004T v3.0**          | Energy monitoring sensor                          | 1–2 |
| **TTL ↔ UART Converter**    | Enables multi-drop Modbus RTU                     | 1   |
| **Active Buzzer**           | Audio alert system                                | 1   |
| **LED Indicators**          | Green (Normal), Red (Alert), Blue (Communication)s| 3   |
| **LCD Display**             | Local data visualization                          | 1   |
| **Power Supply**            | 5V regulated                                      | 1   |

### Hardware Connections diagram

![HARDWARE WIRING DIAGRAM](lib/img/diagram.png)

---

## Software Requirements

- PlatformIO (Arduino Framework)
- ESP32 Board Support (via Arduino Boards Manager)

### Required libraries

- Wire.h

### Cloud Integration

- Blynk
- Thingspeak

---

## 📂 Project Structure

<pre>
Duo-EM-v1.0/
│
├── platformio.ini
│
├── include/
│   └── config.h                 # Global configuration & pin definitions
│
├── lib/
│   ├── GSMModule/
│   │   ├── GSMModule.h           # Class declaration for GSM handling
│   │   └── GSMModule.cpp         # Class implementation
│   │
│   ├── DisplayHandler/
│   │   ├── DisplayHandler.h      # LCD handling declarations
│   │   └── DisplayHandler.cpp    # Implementation
│   │
│   ├── SensorHandler/
│   │   ├── SensorHandler.h       # Sensor reading declarations
│   │   └── SensorHandler.cpp     # Implementation
│   │
│   ├── AlertHandler/
│   │   ├── AlertHandler.h        # LED & buzzer alerts
│   │   └── AlertHandler.cpp
│
├── src/
│   ├── main.cpp                  # Main orchestration code
│
│
└── test/                          # Test programs
    ├── test_gsm_main.cpp          # GSM module test (virtual UART)
    ├── test_display_main.cpp      # Display module test
    ├── test_alert_main.cpp        # Alert module test
    ├── test_sensor_main.cpp       # Sensor reading test

</pre>

---

## ⚙️ Installation & Setup

1. **Clone Repository**

  ```bash
    git clone https://github.com/yourusername/smart-energy-monitor.git
    cd smart-energy-monitor
  ```

2. **Install Libraries**

  -- Open platformIO Extension in **VsCode**
  -- Go to Sketch → Include Library → Manage Libraries
  -- Install required dependencies

3. Configure Settings
  -- Edit **config.h** to match your hardware pins and preferences.
  -- Upload Firmware
  -- Select ESP32 DevKit from Tools → Board
  -- Upload via USB

4. Connect Hardware
  -- Wire components as per the circuit diagram in /docs/wiring_diagram.png
  
---

## 📄  COMPREHENSIVE DIAGNOSTICS SYSTEM COMMANDS

  GSM DIAGNOSTICS:
  gsm_test      - Full GSM module test
  gsm_status    - Show detailed GSM status
  gsm_signal    - Check signal strength
  gsm_network   - Show network information

SMS TESTING:
  sms_send **<number> <message>** - Send test SMS
  sms_test_all  - Send test SMS to all recipients
  sms_simulate <command> - Simulate user SMS command
  sms_stats     - Show SMS statistics

INTERNET TESTING:
  gprs_test     - Test GPRS connection
  gprs_status   - Show GPRS status
  cloud_test    - Test cloud data upload
  http_test <url> - Test HTTP request
  thingspeak_test - Test ThingSpeak upload

USER SIMULATION:
  user_status   - Send status report to users
  user_report   - Send daily report to users
  user_help     - Show SMS commands for users
  emergency_alert - Send emergency test alert

SYSTEM DIAGNOSTICS:
  status        - Complete system status
  sensor_test   - Test PZEM sensors
  threshold_test - Simulate threshold alerts
  memory_info   - Show memory usage
  uptime        - Show system uptime
  full_diag     - Run complete diagnostics
  reset_counters - Reset all statisticS

BASIC COMMANDS:
  test/diag     - Basic sensor diagnostics
  discover      - Discover PZEM addresses
  help          - Show this menu

TIP: All commands are case-insensitive

---

📜 License
This project is licensed under the MIT License – see the LICENSE file for details.

🤝 Contribution
  -We welcome community contributions!
  -Report issues via the GitHub Issues tab.
  -Submit pull requests for bug fixes or feature additions.
  -Improve documentation for better adoption.

---

## 📧 Contact Maintainer

- **Name:** Saviour Dagadu – Embedded Hardware Designer  
- **📍 Location:** Accra, Ghana  
- **✉️ Email:** [senamdagadusaviour@gmail.com](mailto:senamdagadusaviour@gmail.com)  
- **🔗 GitHub:** [SaviourDagadu](https://github.com/SaviourDagadu)
