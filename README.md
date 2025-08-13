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
- **ESP32-DevKit Control** running **MicroPython** firmware.
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
├── src/
│   ├── main.cpp             # Main firmware logic
│   ├── config.h             # System configuration
│   ├── alert_handler.cpp    # Buzzer and LED management
│   ├── display_handler.cpp  # OLED display rendering
│   ├── pzem_handler.cpp    # PZEM-004T readings
│   ├── comms_handler.cpp    # Cloud/GSM/Blynk integration
│
├── docs/
│   ├── wiring_diagram.png   # Circuit wiring reference
│   ├── protocol_notes.md    # Modbus communication notes
│
├── README.md                # Project documentation
├── LICENSE                  # Open source license
</pre>

---

## ⚙️ Installation & Setup

1. **Clone Repository**

  ```bash
    git clone https://github.com/yourusername/smart-energy-monitor.git
    cd smart-energy-monitor
  ```

2. **Install Libraries**
  - Open platformIO Extension in **VsCode** 
  - Go to Sketch → Include Library → Manage Libraries
  - Install required dependencies

3. Configure Settings
  - Edit **config.h** to match your hardware pins and preferences.
  - Upload Firmware
  - Select ESP32 DevKit from Tools → Board
  - Upload via USB

4. Connect Hardware
  - Wire components as per the circuit diagram in /docs/wiring_diagram.png
  
---

📜 License
This project is licensed under the MIT License – see the LICENSE file for details.

🤝 Contribution
  - We welcome community contributions!
  - Report issues via the GitHub Issues tab.
  - Submit pull requests for bug fixes or feature additions.
  - Improve documentation for better adoption.

---
## 📧 Contact Maintainer

- **Name:** Saviour Dagadu – Embedded Hardware Designer  
- **📍 Location:** Accra, Ghana  
- **✉️ Email:** [senamdagadusaviour@gmail.com](mailto:senamdagadusaviour@gmail.com)  
- **🔗 GitHub:** [SaviourDagadu](https://github.com/SaviourDagadu)



