/*
 =============================================================================
  CONFIGURATION HEADER FILE
 =============================================================================
  File: config.h
  Project: Dual-Tenant Energy Monitoring System
  Platform: ESP32 DevKit (Arduino Framework, PlatformIO)
  Author: Saviour Dagadu (Embedded Hardware Designer) - Accra, Ghana
  License: MIT
 =============================================================================
  DESCRIPTION:
  ---------------------------------------------------------------------------
  This configuration header centralizes all system constants, pin assignments,
  communication settings, and runtime parameters used across the firmware.

  The Dual-Tenant Energy Monitoring System is an IoT-ready platform designed
  to measure, monitor, and analyze the energy consumption of two independent
  tenants (or circuits) in real time. It integrates PZEM-004T v3.0 energy 
  meters, an LCD display, LED indicators, an active buzzer for alerts, 
  and GSM/Wi-Fi connectivity for remote data reporting and SMS notifications.

  The project supports:
    - Multi-UART management for PZEM and GSM modules
    - I2C-based LCD and RTC for local display and timestamping
    - Configurable energy and cost thresholds for alerts
    - Modular and maintainable codebase for scalability

  This file ensures that:
    1. All hardware pin mappings are defined in one location for easy changes.
    2. Communication parameters (UART/I2C/Wi-Fi) are consistent across modules.
    3. System thresholds, intervals, and credentials can be adjusted without
       modifying core logic files.

  KEY HARDWARE COMPONENTS:
    - ESP32 DevKit V1 (Main MCU, Wi-Fi + Bluetooth)
    - 2x PZEM-004T v3.0 Energy Monitoring Modules
    - SIM800L GSM Module (SMS & Cloud Upload via GSM)
    - LCD Display (I2C)
    - RTC Module (DS3231 or DS1307)
    - Active Buzzer (Alert Sound)
    - LED Indicators (Normal/Alert/Communication)
    - 5V Regulated Power Supply

  SAFE GPIO MAPPING (ESP32 DEVKIT):
    PZEM A TX   -> GPIO26
    PZEM A RX   -> GPIO27
    PZEM B TX   -> GPIO14
    PZEM B RX   -> GPIO12
    GSM TX      -> GPIO16
    GSM RX      -> GPIO17
    LCD/RTC SDA -> GPIO21
    LCD/RTC SCL -> GPIO22
    BUZZER      -> GPIO18
    LED GREEN   -> GPIO19
    LED RED     -> GPIO23
    LED BLUE    -> GPIO25

  NOTE:
    - All timing intervals are in milliseconds unless stated otherwise.
    - Wi-Fi and GSM credentials are stored here for development convenience,
      but for production, sensitive data should be moved to secure storage.
    - Changing pin assignments here will automatically propagate across all
      modules that include config.h.

 =============================================================================
*/

#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------
// General Runtime Settings
// -----------------------------
#define PROJECT_NAME           "dual_tenant_energy_monitor"
#define VERSION                "0.1.0"
#define DEBUG_MODE             true

// -----------------------------
// UART & PZEM Settings
// -----------------------------
// Tenant A
#define PZEM_A_TX_PIN          26
#define PZEM_A_RX_PIN          27

// Tenant B
#define PZEM_B_TX_PIN          14
#define PZEM_B_RX_PIN          12

#define PZEM_UART_BAUDRATE     9600
#define PZEM_UART_CONFIG       SERIAL_8N1

// -----------------------------
// GSM SIM800L Settings
// -----------------------------
#define GSM_TX_PIN             16
#define GSM_RX_PIN             17
#define GSM_UART_BAUDRATE      9600
#define GSM_UART_CONFIG        SERIAL_8N1

// -----------------------------
// I2C Settings (LCD + RTC)
// -----------------------------
#define I2C_SDA_PIN            21
#define I2C_SCL_PIN            22
#define I2C_FREQ               400000UL

#define LCD_I2C_ADDR           0x27
#define LCD_COLS               16
#define LCD_ROWS               4
#define RTC_I2C_ADDR           0x68

// -----------------------------
// Indicators & Alerts
// -----------------------------
#define BUZZER_PIN             18
#define LED_GREEN_PIN          19
#define LED_RED_PIN            23
#define LED_BLUE_PIN           25

// -----------------------------
// Energy & Cost Settings
// -----------------------------
#define ENERGY_RATE_GHS        1.60f
#define DAILY_ENERGY_THRESHOLD 30.0f
#define DAILY_COST_THRESHOLD   48.0f

// -----------------------------
// Timing Intervals (ms)
// -----------------------------
#define SENSOR_READ_INTERVAL   1000
#define DISPLAY_UPDATE_INTERVAL 2000
#define DATA_LOG_INTERVAL      60000
#define SMS_CHECK_INTERVAL     300000
#define API_UPDATE_INTERVAL    300000
#define DISPLAY_PAGE_DURATION  5000

// -----------------------------
// Networking / Cloud
// -----------------------------
#define THINGSPEAK_API_KEY     "YOUR_THINGSPEAK_WRITE_API_KEY"
#define THINGSPEAK_CHANNEL_ID  "YOUR_CHANNEL_ID"

#define WIFI_SSID              "YOUR_WIFI_SSID"
#define WIFI_PASSWORD          "YOUR_WIFI_PASSWORD"

static const char* SMS_RECIPIENTS[] = {
    "+233XXXXXXXXX",
    "+233YYYYYYYYY"
};
#define SMS_RECIPIENT_COUNT    (sizeof(SMS_RECIPIENTS) / sizeof(SMS_RECIPIENTS[0]))

#define WEB_SERVER_PORT        80
#define PRIVATE_API_ENABLED    true

#endif // CONFIG_H
