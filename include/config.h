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

// ===================================
// DEBUG AND SYSTEM CONFIGURATION
// ===================================
#define DEBUG_MODE true
#define SYSTEM_VERSION "1.2.0"
#define DEVICE_ID "ESM_001"  // Energy System Monitor

// ===================================
// GSM/SIM800L CONFIGURATION
// ===================================
#define USE_UART2_FOR_GSM true

// GPIO Pin Definitions
#define GSM_TX_PIN 16
#define GSM_RX_PIN 17
#define GSM_RESET_PIN 5     // Optional reset pin
#define GSM_PWR_PIN 4       // Optional power control pin

// UART Configuration
#define GSM_UART_BAUDRATE      9600
#define GSM_UART_CONFIG        SERIAL_8N1

//#define UART_BUFFER_SIZE       256      // UART receive buffer size

// Network Configuration
#define GSM_APN "internet"  // Change to your carrier's APN
#define GSM_USERNAME ""     // Usually empty for most carriers
#define GSM_PASSWORD ""     // Usually empty for most carriers

// SMS Configuration
static const char* const SMS_RECIPIENTS[] = {
    "+233205324322",    // First recipient (Tenant_A)
    "+233245829456",    // Second recipient (Tenant_B)
    "+233524919044"     // Emergency contact (LandLord)
};
#define SMS_RECIPIENT_COUNT (sizeof(SMS_RECIPIENTS) / sizeof(SMS_RECIPIENTS[0]))

// SMS Rate Limiting
#define SMS_MIN_INTERVAL 30000    // 30 seconds between SMS
#define SMS_RETRY_COUNT 3         // Number of SMS retry attempts
#define SMS_TIMEOUT 30000         // SMS send timeout (30 seconds)

// ===================================
// CLOUD/API CONFIGURATION
// ===================================
#define THINGSPEAK_API_KEY "F4SQUSOSHFE7K3I7&field1=0"
#define THINGSPEAK_CHANNEL_ID "3035836"

// HTTP Configuration
#define HTTP_TIMEOUT 30000        // HTTP request timeout (30 seconds)
#define HTTP_RETRY_COUNT 3        // Number of HTTP retry attempts
#define HTTP_USER_AGENT "ESP32-EnergyMonitor/1.0"

// Cloud Services URLs
#define THINGSPEAK_UPDATE_URL "https://api.thingspeak.com/update"
#define BACKUP_CLOUD_URL "https://your-backup-service.com/api/data"

// ===================================
// SENSOR CONFIGURATION (PZEM-004T)
// ===================================
#define PZEM_UART_BAUDRATE    9600
#define PZEM_UART_CONFIG      SERIAL_8N1

// Tenant A (Unit A) Configuration
#define PZEM_A_TX_PIN          26
#define PZEM_A_RX_PIN          27
#define PZEM_A_ADDRESS         0x01
#define PZEM_A_SOFTWARE_SERIAL true    // Using SoftwareSerial

// Tenant B (Unit B) Configuration  
#define PZEM_B_TX_PIN          14
#define PZEM_B_RX_PIN          12
#define PZEM_B_ADDRESS         0x01
#define PZEM_B_SOFTWARE_SERIAL true    // Using SoftwareSerial

// Sensor Timing
#define SENSOR_READ_INTERVAL 5000      // Read sensors every 5 seconds
#define PZEM_RETRY_COUNT 3           // Retry failed sensor reads
#define PZEM_RESPONSE_TIMEOUT 2000            // Sensor communication timeout

// ===================================
// ENERGY MONITORING THRESHOLDS
// ===================================
#define DAILY_ENERGY_THRESHOLD 25.0    // kWh per day limit
#define DAILY_COST_THRESHOLD 15.00     // Cost limit (in your currency)
#define ENERGY_COST_PER_KWH 1.60       // Cost per kWh (adjust for your rate)

// Alert Hysteresis (to prevent alert flapping)
#define ALERT_HYSTERESIS_PERCENT 10    // 10% below threshold to clear alert

// Power Quality Thresholds
#define MIN_VOLTAGE 200.0              // Minimum acceptable voltage
#define MAX_VOLTAGE 250.0              // Maximum acceptable voltage
#define MAX_CURRENT 25.0               // Maximum current per tenant
#define MAX_POWER 5500.0               // Maximum power per tenant (watts)

// ===================================
// TIMING INTERVALS
// ===================================
#define DATA_LOG_INTERVAL 300000       // Log to cloud every 5 minutes
#define SMS_CHECK_INTERVAL 60000       // Check for SMS every 1 minute
#define API_UPDATE_INTERVAL 600000     // API updates every 10 minutes
#define DAILY_RESET_INTERVAL 86400000  // Reset daily counters (24 hours)
#define SYSTEM_HEALTH_CHECK 300000     // System health check every 5 minutes

// Diagnostic Intervals
#define DIAGNOSTIC_INTERVAL 3600000    // Auto-diagnostics every hour
#define STATUS_REPORT_INTERVAL 21600000 // Status report every 6 hours

// ===================================
// LCD DISPLAY & RTC(NTP) CONFIGURATION
// ===================================
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define LCD_I2C_ADDR 0x27
#define RTC_I2C_ADDR    0x68
#define LCD_COLS 16
#define LCD_ROWS 4

// Display Update Intervals
#define DISPLAY_PAGE_DURATION 2000       // Update LCD every 2 seconds
#define LCD_ALERT_BLINK_INTERVAL 500   // Alert blink rate
#define LCD_BACKLIGHT_TIMEOUT 300000   // Turn off backlight after 5 minutes

// ===================================
// ALERT SYSTEM CONFIGURATION
// ===================================
// LED Pin Definitions
#define LED_GREEN_PIN 19          // Green LED - System OK
#define LED_RED_PIN 23            // Red LED - Energy threshold exceeded
#define LED_RED_PIN 23            // Red LED - System error
#define LED_BLUE_PIN 25           // Blue LED - Communication activity

// Buzzer Configuration
#define BUZZER_PIN 18
#define BUZZER_FREQUENCY 2000    // 2kHz tone
#define BUZZER_DURATION 200      // 200ms beep

// Alert Patterns
#define ALERT_BLINK_FAST 250     // Fast blink (250ms)
#define ALERT_BLINK_SLOW 1000    // Slow blink (1000ms)
#define ALERT_SOLID_ON 0         // Solid on

// ===================================
// SYSTEM LIMITS AND BUFFERS
// ===================================
#define MAX_SMS_LENGTH 160           // Standard SMS length
#define MAX_HTTP_RESPONSE 1024       // Maximum HTTP response size
#define MAX_AT_RESPONSE 512          // Maximum AT command response
#define MAX_BUFFERED_READINGS 50     // Maximum buffered sensor readings
#define MAX_ERROR_LOG_ENTRIES 10     // Maximum error log entries

// Memory Management
#define WATCHDOG_TIMEOUT 30000       // 30 second watchdog timeout
#define STACK_SIZE_GSM 8192          // Stack size for GSM task
#define STACK_SIZE_SENSORS 4096      // Stack size for sensor task

// ===================================
// SECURITY AND AUTHENTICATION
// ===================================
// Authorized Numbers for SMS Commands (same as recipients for now)
#define AUTHORIZED_NUMBERS SMS_RECIPIENTS
#define MAX_SMS_COMMANDS_PER_HOUR 10     // Rate limiting for SMS commands

// Device Security
#define ENABLE_SMS_ENCRYPTION false      // SMS encryption (future feature)
#define REQUIRE_SMS_AUTHENTICATION true  // Require authorized numbers
#define LOG_UNAUTHORIZED_ACCESS true     // Log unauthorized SMS attempts

// ===================================
// ADVANCED FEATURES
// ===================================
// Power Management
#define ENABLE_SLEEP_MODE false          // Sleep mode when inactive
#define SLEEP_TIMEOUT 1800000           // Sleep after 30 minutes inactive
#define ENABLE_POWER_SAVING true        // General power saving features

// Data Management  
#define ENABLE_DATA_COMPRESSION false   // Compress data before transmission
#define ENABLE_OFFLINE_STORAGE true     // Store data when offline
#define MAX_OFFLINE_STORAGE_DAYS 7      // Maximum days to store offline data

// Diagnostics
#define AUTO_DIAGNOSTIC_ENABLED true    // Automatic system diagnostics
#define DETAILED_LOGGING true           // Detailed debug logging
#define PERFORMANCE_MONITORING true     // Monitor system performance

// ===================================
// ERROR CODES AND MESSAGES
// ===================================
#define ERROR_GSM_INIT_FAILED "E001"
#define ERROR_NETWORK_NOT_REGISTERED "E002"
#define ERROR_SMS_SEND_FAILED "E003"
#define ERROR_GPRS_CONNECTION_FAILED "E004"
#define ERROR_SENSOR_A_COMMUNICATION "E005"
#define ERROR_SENSOR_B_COMMUNICATION "E006"
#define ERROR_THRESHOLD_EXCEEDED "E007"
#define ERROR_MEMORY_LOW "E008"
#define ERROR_WATCHDOG_RESET "E009"
#define ERROR_POWER_QUALITY "E010"

// ===================================
// FEATURE FLAGS
// ===================================
#define ENABLE_TWO_WAY_SMS true         // Enable SMS command processing
#define ENABLE_CLOUD_LOGGING true       // Enable cloud data logging
#define ENABLE_LCD_DISPLAY true         // Enable LCD display
#define ENABLE_AUDIO_ALERTS true        // Enable buzzer alerts
#define ENABLE_LED_INDICATORS true      // Enable LED status indicators
#define ENABLE_DIAGNOSTICS true         // Enable diagnostic system
#define ENABLE_REMOTE_CONFIG false      // Remote configuration (future)
#define ENABLE_OTA_UPDATES false        // Over-the-air updates (future)

// ===================================
// CALIBRATION VALUES
// ===================================
// PZEM Sensor Calibration (adjust if needed)
#define VOLTAGE_CALIBRATION 1.0
#define CURRENT_CALIBRATION 1.0  
#define POWER_CALIBRATION 1.0
#define ENERGY_CALIBRATION 1.0

// Environmental Compensation
#define TEMPERATURE_COMPENSATION false
#define HUMIDITY_COMPENSATION false

// ===================================
// DEVELOPMENT AND TESTING
// ===================================
#ifdef DEBUG_MODE
    #define DEBUG_GSM true
    #define DEBUG_SENSORS true
    #define DEBUG_ALERTS true
    #define DEBUG_CLOUD true
    #define DEBUG_SMS true
    #define DEBUG_MEMORY false
    #define SIMULATE_SENSOR_DATA false   // Use simulated data for testing
    #define FAST_TESTING_INTERVALS false // Use shorter intervals for testing
#else
    #define DEBUG_GSM false
    #define DEBUG_SENSORS false
    #define DEBUG_ALERTS false
    #define DEBUG_CLOUD false
    #define DEBUG_SMS false
    #define DEBUG_MEMORY false
    #define SIMULATE_SENSOR_DATA false
    #define FAST_TESTING_INTERVALS false
#endif

// ===================================
// SYSTEM STATUS MESSAGES
// ===================================
#define MSG_SYSTEM_STARTING "System Starting..."
#define MSG_GSM_INITIALIZING "Initializing GSM..."
#define MSG_SENSORS_READY "Sensors Ready"
#define MSG_CLOUD_CONNECTED "Cloud Connected"
#define MSG_SYSTEM_READY "System Ready"
#define MSG_ALERT_ACTIVE "ALERT ACTIVE"
#define MSG_MAINTENANCE_MODE "Maintenance Mode"

// ===================================
// VALIDATION MACROS
// ===================================
#define VALIDATE_PHONE_NUMBER(num) (strlen(num) >= 10 && strlen(num) <= 15)
#define VALIDATE_ENERGY_VALUE(val) (val >= 0 && val <= 999.99)
#define VALIDATE_VOLTAGE(val) (val >= MIN_VOLTAGE && val <= MAX_VOLTAGE)
#define VALIDATE_CURRENT(val) (val >= 0 && val <= MAX_CURRENT)
#define VALIDATE_POWER(val) (val >= 0 && val <= MAX_POWER)

#endif // CONFIG_H