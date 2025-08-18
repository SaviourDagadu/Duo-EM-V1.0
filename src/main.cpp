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

// Diagnotics & Function  prototypes
void updateAPI();
void checkForIncomingSMS();
void logDataToCloud();
void checkEnergyThresholds(const PZEMResult& energyData);
void printInstructions();

void handleSerialCommands();
void runComprehensiveDiagnostics();
void testSMSFunctionality();
void testGPRSConnectivity();
void testUserInteractionSimulation();
void printDiagnosticsMenu();
void simulateUserSMS(const String& command);
void sendStatusToUsers();
void sendDailyReportToUsers();
void showSystemStatus();

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }    // Wait for serial port to connect

  // Initialize components
  lcdInterface.begin();
  sensorHandler.init();
  alertHandler.begin();

  // Show startup message
  lcdInterface.showSystemMessage("Initializing ...",2000);

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

  printInstructions();
  printDiagnosticsMenu();
}

void loop() {
  unsigned long currentTime = millis();

  //Handle Serial commands first
  handleSerialCommands();

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
    // Determine which sensor failed
    String sensorName = !sensorStatus.tenant_a_ok ? "A" : "B";
    
    // For serial debugging - show raw code
    if (DEBUG_MODE) {
        Serial.print("Sensor ");
        Serial.print(sensorName);
        Serial.print(" error: ");
        Serial.println(sensorStatus.last_error);
    }
    
    // Trigger visual/audio alerts
    alertHandler.triggerSystemAlert();
    lcdInterface.showAlert(sensorStatus.last_error); // Pass raw code to LCD
    
    // Send detailed SMS if ready
    if (!systemAlertSent && gsmModule.getStatus().smsReady) {
        String smsMsg = "UNIT " + sensorName + " error: ";
        
        // Add description for SMS
        if (sensorStatus.last_error == "E1") {
            smsMsg += "UNIT A communication failure";
        } 
        else if (sensorStatus.last_error == "E2") {
            smsMsg += "UNIT B communication failure";
        }
        else {
            smsMsg += sensorStatus.last_error; // fallback
        }
        
        if (gsmModule.sendSystemAlert(smsMsg)) {
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
  if (currentTime - lastDailyResetCheck >=86400000) { //  86400000 24 hours
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

  // Check Screen Mode.
  lcdInterface.backLightMode();

  // Small delay to prevent watchdog 
  delay(10);
}

////////////  FUNCTIONS PROTOTYPES ////////////////////

// Serial Command Handler
void handleSerialCommands() {
  if (!Serial.available()) return;
  
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (DEBUG_MODE) {
    Serial.println("Received command: " + command);
  }

  // Basic diagnostic commands
  if (command == "test" || command == "diag") {
    Serial.println("Running basic diagnostics...");
    sensorHandler.runDiagnostics();
  }
  else if (command == "help") {
    printDiagnosticsMenu();
  }
  else if (command == "discover") {
    Serial.println("Discovering PZEM devices...");
    uint8_t foundA = sensorHandler.discoverAddresses(1);
    uint8_t foundB = sensorHandler.discoverAddresses(2);
    Serial.print("Total devices found: ");
    Serial.println(foundA + foundB);
  }

    // NGSM Diagnostic Commands
  else if (command == "gsm_test") {
    Serial.println("Running GSM diagnostics...");
    gsmModule.runFullDiagnostics();
  }
  else if (command == "gsm_status") {
    gsmModule.printDetailedStatus();
  }
  else if (command == "gsm_signal") {
    GSMModule::ModuleStatus status = gsmModule.getStatus();
    Serial.println("Signal: " + String(status.signalStrength) + "/5 (" + gsmModule.getSignalQualityDescription() + ")");
  }
  else if (command == "gsm_network") {
    GSMModule::ModuleStatus status = gsmModule.getStatus();
    Serial.println("Network: " + status.operatorName + " (" + 
                  (status.networkRegistered ? "Registered" : "Not Registered") + ")");
  }

  // SMS Testing Commands
  else if (command.startsWith("sms_send ")) {
    int firstSpace = command.indexOf(' ', 9);
    if (firstSpace != -1) {
      String number = command.substring(9, firstSpace);
      String message = command.substring(firstSpace + 1);
      Serial.println("📱 Sending test SMS...");
      if (gsmModule.sendSMS(number, message)) {
        Serial.println("✓ SMS sent successfully");
      } else {
        Serial.println("✗ SMS failed");
      }
    } else {
      Serial.println("Usage: sms_send <number> <message>");
    }
  }
  else if (command == "sms_test_all") {
    Serial.println("📱 Sending test SMS to all recipients...");
    String testMsg = "Test SMS from diagnostics - " + gsmModule.getTimestamp();
    if (gsmModule.sendSMSToRecipients(testMsg)) {
      Serial.println("✓ Test SMS sent to all recipients");
    } else {
      Serial.println("✗ Failed to send test SMS");
    }
  }
  else if (command.startsWith("sms_simulate ")) {
    String smsCommand = command.substring(13);
    simulateUserSMS(smsCommand);
  }
  else if (command == "sms_stats") {
    GSMModule::ModuleStatus status = gsmModule.getStatus();
    Serial.println("SMS Statistics:");
    Serial.println("  Sent: " + String(status.smsSentCount));
    Serial.println("  Failed: " + String(status.smsFailedCount));
    Serial.println("  Received: " + String(status.smsReceivedCount));
  }
  
  // GPRS Testing Commands
  else if (command == "gprs_test" || command == "gprs_connect") {
    Serial.println("Testing GPRS connection...");
    if (gsmModule.setupGPRS()) {
      Serial.println("✓ GPRS connection successful");
    } else {
      Serial.println("✗ GPRS connection failed");
    }
  }
  else if (command == "gprs_status") {
    GSMModule::ModuleStatus status = gsmModule.getStatus();
    Serial.println("GPRS Status: " + String(status.gprsConnected ? "Connected" : "Disconnected"));
    if (status.ipAddress.length() > 0) {
      Serial.println("IP Address: " + status.ipAddress);
    }
  }
  else if (command == "cloud_test") {
    Serial.println("Testing cloud upload...");
    logDataToCloud();
  }
  else if (command.startsWith("http_test ")) {
    String url = command.substring(10);
    Serial.println("Testing HTTP request to: " + url);
    if (gsmModule.sendHTTPRequest(url)) {
      Serial.println("✓ HTTP request successful");
    } else {
      Serial.println("✗ HTTP request failed");
    }
  }
  else if (command == "thingspeak_test") {
    Serial.println("Testing ThingSpeak upload...");
    // Generate test data
    String url = "https://api.thingspeak.com/update?api_key" + String(THINGSPEAK_API_KEY);
    url += "&field1=230.5&field2=1.2&field3=276.6&field4=1.5";
    url += "&field5=231.2&field6=0.8&field7=184.9&field8=1.2";
    
    if (gsmModule.sendHTTPRequest(url)) {
      Serial.println("✓ ThingSpeak test successful");
    } else {
      Serial.println("✗ ThingSpeak test failed");
    }
  }
  
  // User Interaction Simulation
  else if (command == "user_status") {
    sendStatusToUsers();
  }
  else if (command == "user_report") {
    sendDailyReportToUsers();
  }
  else if (command == "user_help") {
    Serial.println("  SMS Commands available to users:");
    Serial.println("  STATUS - Get current system status");
    Serial.println("  REPORT - Get daily energy report");
    Serial.println("  SIGNAL - Check signal strength");
    Serial.println("  RESET COUNTERS - Reset statistics");
    Serial.println("  HELP - Show available commands");
  }
  else if (command == "emergency_alert") {
    Serial.println("Sending emergency alert...");
    if (gsmModule.sendSystemAlert("EMERGENCY TEST - System functioning normally")) {
      Serial.println("✓ Emergency alert sent");
    } else {
      Serial.println("✗ Emergency alert failed");
    }
  }
  
  // System Commands
  else if (command == "sys_status" || command == "status") {
    showSystemStatus();
  }
  else if (command == "sensor_test") {
    Serial.println("Testing PZEM sensors...");
    sensorHandler.runDiagnostics();
  }
  else if (command == "threshold_test") {
    Serial.println("Simulating threshold alerts...");
    PZEMResult testData = sensorHandler.readAll();
    testData.tenant_a.daily_energy_kwh = DAILY_ENERGY_THRESHOLD + 1.0;
    checkEnergyThresholds(testData);
  }
  else if (command == "memory_info") {
    Serial.println("  Memory Information:");
    Serial.println("  Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("  Heap Size: " + String(ESP.getHeapSize()) + " bytes");
    Serial.println("  Free PSRAM: " + String(ESP.getFreePsram()) + " bytes");
  }
  else if (command == "uptime") {
    unsigned long uptime = millis();
    Serial.println("System Uptime: " + String(uptime / 1000) + " seconds");
    Serial.println("   (" + String(uptime / 60000) + " minutes)");
  }
  else if (command == "full_diag") {
    runComprehensiveDiagnostics();
  }
  else if (command == "reset_counters") {
    gsmModule.resetCounters();
    Serial.println("All counters reset");
  }
  
  else {
    Serial.println("Unknown command. Type 'help' for available commands.");
  }
}

// ***************Comprehensive Diagnostics Function*****************//
void runComprehensiveDiagnostics() {
  Serial.println("\n" + String("=").substring(0,60));
  Serial.println("COMPREHENSIVE SYSTEM DIAGNOSTICS");
  Serial.println(String("=").substring(0,60));
  
  // System Information
  Serial.println("SYSTEM INFORMATION:");
  Serial.println("  Uptime: " + String(millis() / 1000) + " seconds");
  Serial.println("  Free Memory: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("  ESP32 Chip: " + String(ESP.getChipModel()));
  
  // GSM Module Diagnostics
  Serial.println("\nGSM MODULE DIAGNOSTICS:");
  bool gsmOK = gsmModule.runFullDiagnostics();
  
  // Sensor Diagnostics
  Serial.println("\nSENSOR DIAGNOSTICS:");
  Serial.println("Running PZEM sensor tests...");
  sensorHandler.runDiagnostics();
  
  // Test SMS Functionality
  Serial.println("\nSMS FUNCTIONALITY TEST:");
  GSMModule::ModuleStatus gsmStatus = gsmModule.getStatus();
  if (gsmStatus.smsReady) {
    Serial.println("✓ SMS module ready");
    Serial.println("  SMS Sent: " + String(gsmStatus.smsSentCount));
    Serial.println("  SMS Failed: " + String(gsmStatus.smsFailedCount));
    Serial.println("  SMS Received: " + String(gsmStatus.smsReceivedCount));
  } else {
    Serial.println("✗ SMS module not ready");
  }
  
  // Test GPRS Functionality
  Serial.println("\nGPRS FUNCTIONALITY TEST:");
  if (gsmStatus.gprsConnected) {
    Serial.println("✓ GPRS connected");
  } else {
    Serial.println("GPRS not connected - testing connection...");
    if (gsmModule.setupGPRS()) {
      Serial.println("✓ GPRS connection established");
    } else {
      Serial.println("✗ GPRS connection failed");
    }
  }
  
  // Overall System Health
  Serial.println("\nOVERALL SYSTEM HEALTH:");
  bool systemHealthy = gsmOK && gsmStatus.networkRegistered && (gsmStatus.signalStrength > 1);
  
  if (systemHealthy) {
    Serial.println("SYSTEM HEALTHY - All major components functioning");
  } else {
    Serial.println("SYSTEM ISSUES DETECTED:");
    if (!gsmOK) Serial.println("  - GSM module issues");
    if (!gsmStatus.networkRegistered) Serial.println("  - Network registration failed");
    if (gsmStatus.signalStrength <= 1) Serial.println("  - Poor signal strength");
  }
  
  Serial.println(String("=").substring(0,60));
  Serial.println("DIAGNOSTICS COMPLETE");
  Serial.println(String("=").substring(0,60) + "\n");
}

// User Interaction Simulation Functions
void simulateUserSMS(const String& command) {
  Serial.println("📱 Simulating SMS from user: '" + command + "'");
  
  GSMModule::SMSCommand cmd = gsmModule.parseSMSCommand(command, SMS_RECIPIENTS[0]);
  
  if (cmd.isValid) {
    Serial.println("✓ Command valid: " + cmd.command);
    if (gsmModule.processSMSCommand(cmd)) {
      Serial.println("✓ Command processed successfully");
    } else {
      Serial.println("✗ Command processing failed");
    }
  } else {
    Serial.println("✗ Invalid command");
    Serial.println("Available commands: STATUS, REPORT, SIGNAL, RESET COUNTERS, HELP");
  }
}

void sendStatusToUsers() {
  Serial.println("Sending status report to users...");
  String statusMsg = gsmModule.generateStatusResponse();
  
  if (gsmModule.sendSMSToRecipients(statusMsg)) {
    Serial.println("✓ Status report sent to all users");
  } else {
    Serial.println("✗ Failed to send status report");
  }
}

void sendDailyReportToUsers() {
  Serial.println("Sending daily report to users...");
  PZEMResult energyData = sensorHandler.readAll();
  
  if (gsmModule.sendDailyReport(
      energyData.tenant_a.daily_energy_kwh, 
      energyData.tenant_a.daily_energy_kwh * ENERGY_COST_PER_KWH,
      energyData.tenant_b.daily_energy_kwh, 
      energyData.tenant_b.daily_energy_kwh * ENERGY_COST_PER_KWH)) {
    Serial.println("✓ Daily report sent to all users");
  } else {
    Serial.println("✗ Failed to send daily report");
  }
}

void showSystemStatus() {
  Serial.println("\nCOMPLETE SYSTEM STATUS:");
  Serial.println("============================");
  
  // GSM Status
  GSMModule::ModuleStatus gsmStatus = gsmModule.getStatus();
  Serial.println("GSM MODULE:");
  Serial.println("  Ready: " + String(gsmStatus.moduleReady ? "YES" : "NO"));
  Serial.println("  Network: " + String(gsmStatus.networkRegistered ? "Registered" : "Not Registered"));
  Serial.println("  Signal: " + String(gsmStatus.signalStrength) + "/5");
  Serial.println("  Operator: " + gsmStatus.operatorName);
  Serial.println("  SMS Ready: " + String(gsmStatus.smsReady ? "YES" : "NO"));
  Serial.println("  GPRS: " + String(gsmStatus.gprsConnected ? "Connected" : "Disconnected"));
  Serial.println("  SMS Sent: " + String(gsmStatus.smsSentCount));
  Serial.println("  SMS Failed: " + String(gsmStatus.smsFailedCount));
  Serial.println("  SMS Received: " + String(gsmStatus.smsReceivedCount));
  
  // Sensor Status
  StatusResult sensorStatus = sensorHandler.getStatus();
  PZEMResult energyData = sensorHandler.readAll();
  Serial.println("\n🔌 SENSOR STATUS:");
  Serial.println("  Tenant A: " + String(sensorStatus.tenant_a_ok ? "OK" : "ERROR"));
  Serial.println("  Tenant B: " + String(sensorStatus.tenant_b_ok ? "OK" : "ERROR"));
  if (!sensorStatus.tenant_a_ok || !sensorStatus.tenant_b_ok) {
    Serial.println("  Last Error: " + sensorStatus.last_error);
  }
  
  // Energy Data
  Serial.println("\n⚡ CURRENT ENERGY DATA:");
  Serial.println("  TENANT A:");
  Serial.println("    Voltage: " + String(energyData.tenant_a.voltage, 1) + "V");
  Serial.println("    Current: " + String(energyData.tenant_a.current, 2) + "A");
  Serial.println("    Power: " + String(energyData.tenant_a.power, 1) + "W");
  Serial.println("    Daily Energy: " + String(energyData.tenant_a.daily_energy_kwh, 2) + "kWh");
  
  Serial.println("  TENANT B:");
  Serial.println("    Voltage: " + String(energyData.tenant_b.voltage, 1) + "V");
  Serial.println("    Current: " + String(energyData.tenant_b.current, 2) + "A");
  Serial.println("    Power: " + String(energyData.tenant_b.power, 1) + "W");
  Serial.println("    Daily Energy: " + String(energyData.tenant_b.daily_energy_kwh, 2) + "kWh");
  
  // System Health
  Serial.println("\n SYSTEM HEALTH:");
  Serial.println("  Uptime: " + String(millis() / 60000) + " minutes");
  Serial.println("  Free Memory: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("  Alert Status:");
  Serial.println("    Energy Alert: " + String(energyAlertSent ? "ACTIVE" : "CLEAR"));
  Serial.println("    Cost Alert: " + String(costAlertSent ? "ACTIVE" : "CLEAR"));
  Serial.println("    System Alert: " + String(systemAlertSent ? "ACTIVE" : "CLEAR"));
  
  Serial.println("============================\n");
}

void printDiagnosticsMenu() {
    Serial.println("\n" + String("=").substring(0,70));
    Serial.println(" COMPREHENSIVE DIAGNOSTICS SYSTEM");
    Serial.println(String("=").substring(0,70));
    
    Serial.println("  GSM DIAGNOSTICS:");
    Serial.println("  gsm_test      - Full GSM module test");
    Serial.println("  gsm_status    - Show detailed GSM status");
    Serial.println("  gsm_signal    - Check signal strength");
    Serial.println("  gsm_network   - Show network information");
    
    Serial.println("\nSMS TESTING:");
    Serial.println("  sms_send <number> <message> - Send test SMS");
    Serial.println("  sms_test_all  - Send test SMS to all recipients");
    Serial.println("  sms_simulate <command> - Simulate user SMS command");
    Serial.println("  sms_stats     - Show SMS statistics");
    
    Serial.println("\nINTERNET TESTING:");
    Serial.println("  gprs_test     - Test GPRS connection");
    Serial.println("  gprs_status   - Show GPRS status");
    Serial.println("  cloud_test    - Test cloud data upload");
    Serial.println("  http_test <url> - Test HTTP request");
    Serial.println("  thingspeak_test - Test ThingSpeak upload");
    
    Serial.println("\nUSER SIMULATION:");
    Serial.println("  user_status   - Send status report to users");
    Serial.println("  user_report   - Send daily report to users");
    Serial.println("  user_help     - Show SMS commands for users");
    Serial.println("  emergency_alert - Send emergency test alert");
    
    Serial.println("\nSYSTEM DIAGNOSTICS:");
    Serial.println("  status        - Complete system status");
    Serial.println("  sensor_test   - Test PZEM sensors");
    Serial.println("  threshold_test - Simulate threshold alerts");
    Serial.println("  memory_info   - Show memory usage");
    Serial.println("  uptime        - Show system uptime");
    Serial.println("  full_diag     - Run complete diagnostics");
    Serial.println("  reset_counters - Reset all statistics");
    
    Serial.println("\nBASIC COMMANDS:");
    Serial.println("  test/diag     - Basic sensor diagnostics");
    Serial.println("  discover      - Discover PZEM addresses");
    Serial.println("  help          - Show this menu");
    
    Serial.println(String("=").substring(0,70));
    Serial.println("TIP: All commands are case-insensitive");
    Serial.println(String("=").substring(0,70) + "\n");
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
        if (DEBUG_MODE) Serial.println("✓ Energy alert sent for Tenant A");
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
        if (DEBUG_MODE) Serial.println("✓ Energy alert sent for Tenant B");
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
        if (DEBUG_MODE) Serial.println("✓ Cost alert sent");
      }
    }
  }

  // Clear alerts if below thresholds (with 10% hysteresis)
  if (energyData.tenant_a.daily_energy_kwh <= DAILY_ENERGY_THRESHOLD * 0.9 && 
      energyData.tenant_b.daily_energy_kwh <= DAILY_ENERGY_THRESHOLD * 0.9) {
    alertHandler.clearEnergyAlert();
    if (energyAlertSent) {
      energyAlertSent = false;
      if (DEBUG_MODE) Serial.println("✓ Energy alerts cleared");
    }
  }

  if (energyData.summary.total_daily_cost <= DAILY_COST_THRESHOLD * 0.9) {
    if (costAlertSent) {
      costAlertSent = false;
      if (DEBUG_MODE) Serial.println("✓ Cost alert cleared");
    }
  }
}

void logDataToCloud() {
  if (DEBUG_MODE) Serial.println("Attempting cloud data log...");
  
  PZEMResult energyData = sensorHandler.readAll();
  GSMModule::ModuleStatus gsmStatus = gsmModule.getStatus();
  
  if (gsmStatus.gprsConnected || gsmModule.setupGPRS()) {
    String url = "https://api.thingspeak.com/update?api_key=";
    url += THINGSPEAK_API_KEY;
    url += "&field1=" + String(energyData.tenant_a.voltage, 1);
    url += "&field2=" + String(energyData.tenant_a.current, 2);
    url += "&field3=" + String(energyData.tenant_a.power, 1);
    url += "&field4=" + String(energyData.tenant_a.daily_energy_kwh, 3);
    url += "&field5=" + String(energyData.tenant_b.voltage, 1);
    url += "&field6=" + String(energyData.tenant_b.current, 2);
    url += "&field7=" + String(energyData.tenant_b.power, 1);
    url += "&field8=" + String(energyData.tenant_b.daily_energy_kwh, 3);
    
    alertHandler.setCommunicationStatus(true);
    
    // Use retry mechanism for better reliability
    bool success = gsmModule.sendDataWithRetry(url, "", 2);
    
    alertHandler.setCommunicationStatus(false);
    
    if (success && DEBUG_MODE) {
      Serial.println("✓ Cloud update successful");
    } else if (DEBUG_MODE) {
      Serial.println("Cloud update failed - data buffered");
    }
    
    // Try to send any buffered data
    gsmModule.sendBufferedData();
    
  } else if (DEBUG_MODE) {
    Serial.println("GPRS not available - buffering data");
    // Buffer the data for later transmission
    String dataToBuffer = "field1=" + String(energyData.tenant_a.voltage, 1);
    dataToBuffer += "&field2=" + String(energyData.tenant_a.current, 2);
    dataToBuffer += "&field3=" + String(energyData.tenant_a.power, 1);
    dataToBuffer += "&field4=" + String(energyData.tenant_a.daily_energy_kwh, 3);
    dataToBuffer += "&field5=" + String(energyData.tenant_b.voltage, 1);
    dataToBuffer += "&field6=" + String(energyData.tenant_b.current, 2);
    dataToBuffer += "&field7=" + String(energyData.tenant_b.power, 1);
    dataToBuffer += "&field8=" + String(energyData.tenant_b.daily_energy_kwh, 3);
    
    gsmModule.bufferDataForLater(dataToBuffer);
  }
}

void checkForIncomingSMS() {
  if (DEBUG_MODE && false) { // Set to true for verbose SMS checking
    Serial.println("Checking for incoming SMS...");
  }
  
  String result = gsmModule.parseIncomingSMS();
  
  if (result.length() > 0 && DEBUG_MODE) {
    Serial.println("SMS Processing Result: " + result);
  }
}

void updateAPI() {
  // Try to send any buffered data when doing API updates
  if (gsmModule.getStatus().gprsConnected) {
    gsmModule.sendBufferedData();
  }
  
  if (DEBUG_MODE && false) { // Set to true for verbose API updates
    Serial.println("Performing API updates...");
  }
}

void printInstructions() {
  Serial.println("\n" + String("=").substring(0,50));
  Serial.println("🔧 ENERGY MONITORING SYSTEM READY");
  Serial.println(String("=").substring(0,50));
  Serial.println("System Features:");
  Serial.println("• Dual-tenant energy monitoring");
  Serial.println("ESM_001_v1.2.0");
  Serial.println("• SMS alerts & two-way communication");
  Serial.println("• Cloud data logging (ThingSpeak)");
  Serial.println("• Comprehensive diagnostics");
  Serial.println("• Real-time LCD display");
  Serial.println("");
  Serial.println("Quick Commands:");
  Serial.println("• 'help' - Show diagnostic menu");
  Serial.println("• 'status' - Complete system status");
  Serial.println("• 'full_diag' - Run all diagnostics");
  Serial.println("• 'gsm_test' - Test GSM functionality");
  Serial.println(String("=").substring(0,50) + "\n");
}