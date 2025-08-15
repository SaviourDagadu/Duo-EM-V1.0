#include <Arduino.h>
#include "config.h"
#include "SensorHandler.h"
#include "GSMModule.h"
#include "LCDInterface.h"
#include "AlertHandler.h"

// Global instances
SensorHandler sensorHandler;
GSMModule gsmModule;
LCDInterface lcdInterface;
AlertHandler alertHandler;

// Timing variables
unsigned long lastSensorReadTime = 0;
unsigned long lastDataLogTime = 0;
unsigned long lastSMSCheckTime = 0;
unsigned long lastAPIUpdateTime = 0;
unsigned long lastDailyResetCheck = 0;

// Alert tracking
bool energyAlertSent = false;
bool costAlertSent = false;
bool systemAlertSent = false;

// Function prototypes

void updateAPI();
void checkForIncomingSMS();
void logDataToCloud();
void checkEnergyThresholds(const PZEMResult& energyData);

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }    // Wait for serial port to connect

  // Initialize components
  lcdInterface.begin();
  sensorHandler.init();
  alertHandler.begin();

  // Show startup message
  lcdInterface.showSystemMessage("System Initializing ...",2000);
  delay(2000); // Allow splash screen to display

  // Initialize GSM module (non-blocking)
  if (DEBUG_MODE) {
    Serial.println("Initializing GSM module...");
  }
  bool gsmInitialized = gsmModule.initialize();
  
  if (gsmInitialized && DEBUG_MODE) {
    Serial.println("GSM initialized successfully");
    lcdInterface.showSystemMessage("GSM Ready", 1500);
  } else if (DEBUG_MODE) {
    Serial.println("GSM initialization failed");
    lcdInterface.showSystemMessage("GSM Init Failed", 1500);
  }

  // Initial sensor read
  PZEMResult initialReadings = sensorHandler.readAll();
  StatusResult sensorStatus = sensorHandler.getStatus();
  
  // Initial display update
  lcdInterface.updateDisplay(initialReadings, sensorStatus);
  
  // Set initial alert states
  alertHandler.setSystemStatus(sensorStatus.tenant_a_ok && sensorStatus.tenant_b_ok);
}

void loop() {
  unsigned long currentTime = millis();

  // Handle sensor readings at fixed interval
  if (currentTime - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
    lastSensorReadTime = currentTime;
    
    // Read sensor data
    PZEMResult energyData = sensorHandler.readAll();
    StatusResult sensorStatus = sensorHandler.getStatus();

    // Update display
    lcdInterface.updateDisplay(energyData, sensorStatus);

    // Check for system alerts
    if (!sensorStatus.tenant_a_ok || !sensorStatus.tenant_b_ok) {
      String errorMsg = "Sensor error: " + sensorStatus.last_error;
      if (DEBUG_MODE) Serial.println(errorMsg);
      
      alertHandler.triggerSystemAlert();
      lcdInterface.showAlert(errorMsg);
      
      if (!systemAlertSent && gsmModule.getStatus().smsReady) {
        if (gsmModule.sendSystemAlert(errorMsg)) {
          systemAlertSent = true;
        }
      }
    } else {
      alertHandler.clearSystemAlert();
      systemAlertSent = false;
    }

    // Check for energy threshold alerts
    checkEnergyThresholds(energyData);

    // Update communication LED
    alertHandler.setCommunicationStatus(false); // Reset after operations
  }

  // Daily data reset check (once per day)
  if (currentTime - lastDailyResetCheck >= 86400000) { // 24 hours
    lastDailyResetCheck = currentTime;
    sensorHandler.resetDailyCounters();
    energyAlertSent = false;
    costAlertSent = false;
    
    if (DEBUG_MODE) Serial.println("Daily counters reset");
  }

  // Data logging to cloud at fixed interval
  if (currentTime - lastDataLogTime >= DATA_LOG_INTERVAL) {
    lastDataLogTime = currentTime;
    logDataToCloud();
  }

  // SMS alert check at fixed interval
  if (currentTime - lastSMSCheckTime >= SMS_CHECK_INTERVAL) {
    lastSMSCheckTime = currentTime;
    checkForIncomingSMS();
  }

  // API update at fixed interval
  if (currentTime - lastAPIUpdateTime >= API_UPDATE_INTERVAL) {
    lastAPIUpdateTime = currentTime;
    updateAPI();
  }

  // Update alert handler (for blinking LEDs, etc.)
  alertHandler.update();

  // Small delay to prevent watchdog triggers
  delay(10);
}

void checkEnergyThresholds(const PZEMResult& energyData) {
  // Check Tenant A thresholds
  if (energyData.tenant_a.daily_energy_kwh > DAILY_ENERGY_THRESHOLD) {
    alertHandler.triggerEnergyAlert(1);
    lcdInterface.showAlert("Tenant A: Energy Limit!");
    
    if (!energyAlertSent && gsmModule.getStatus().smsReady) {
      if (gsmModule.sendThresholdAlert("A", "energy", 
          energyData.tenant_a.daily_energy_kwh, DAILY_ENERGY_THRESHOLD)) {
        energyAlertSent = true;
      }
    }
  }

  // Check Tenant B thresholds
  if (energyData.tenant_b.daily_energy_kwh > DAILY_ENERGY_THRESHOLD) {
    alertHandler.triggerEnergyAlert(2);
    lcdInterface.showAlert("Tenant B: Energy Limit!");
    
    if (!energyAlertSent && gsmModule.getStatus().smsReady) {
      if (gsmModule.sendThresholdAlert("B", "energy", 
          energyData.tenant_b.daily_energy_kwh, DAILY_ENERGY_THRESHOLD)) {
        energyAlertSent = true;
      }
    }
  }

  // Check cost thresholds
  if (energyData.summary.total_daily_cost > DAILY_COST_THRESHOLD) {
    alertHandler.triggerEnergyAlert(3); // Both tenants
    lcdInterface.showAlert("Total Cost Limit!");
    
    if (!costAlertSent && gsmModule.getStatus().smsReady) {
      if (gsmModule.sendThresholdAlert("Both", "cost", 
          energyData.summary.total_daily_cost, DAILY_COST_THRESHOLD)) {
        costAlertSent = true;
      }
    }
  }

  // Clear alerts if below thresholds
  if (energyData.tenant_a.daily_energy_kwh <= DAILY_ENERGY_THRESHOLD * 0.9 && 
      energyData.tenant_b.daily_energy_kwh <= DAILY_ENERGY_THRESHOLD * 0.9) {
    alertHandler.clearEnergyAlert();
    energyAlertSent = false;
  }

  if (energyData.summary.total_daily_cost <= DAILY_COST_THRESHOLD * 0.9) {
    costAlertSent = false;
  }
}

void logDataToCloud() {
  if (DEBUG_MODE) Serial.println("Attempting cloud data log...");
  
  PZEMResult energyData = sensorHandler.readAll();
  GSMModule::ModuleStatus gsmStatus = gsmModule.getStatus();
  
  if (gsmStatus.gprsConnected) {
    String url = "https://api.thingspeak.com/update?api_key=";
    url += THINGSPEAK_API_KEY;
    url += "&field1=" + String(energyData.tenant_a.voltage);
    url += "&field2=" + String(energyData.tenant_a.current);
    url += "&field3=" + String(energyData.tenant_a.power);
    url += "&field4=" + String(energyData.tenant_a.daily_energy_kwh);
    url += "&field5=" + String(energyData.tenant_b.voltage);
    url += "&field6=" + String(energyData.tenant_b.current);
    url += "&field7=" + String(energyData.tenant_b.power);
    url += "&field8=" + String(energyData.tenant_b.daily_energy_kwh);
    
    alertHandler.setCommunicationStatus(true);
    bool success = gsmModule.sendHTTPRequest(url);
    alertHandler.setCommunicationStatus(false);
    
    if (success && DEBUG_MODE) {
      Serial.println("Cloud update successful");
    } else if (DEBUG_MODE) {
      Serial.println("Cloud update failed");
    }
  } else if (DEBUG_MODE) {
    Serial.println("GPRS not connected - skipping cloud update");
  }
}

void checkForIncomingSMS() {
  // Placeholder for SMS checking functionality
  // Would typically involve parsing incoming messages and responding
  if (DEBUG_MODE) Serial.println("Checking for incoming SMS...");
}

void updateAPI() {
  // Placeholder for any additional API updates
  if (DEBUG_MODE) Serial.println("Performing API updates...");
}