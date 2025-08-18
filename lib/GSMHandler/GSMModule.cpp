#include "GSMModule.h"
#include "config.h"

GSMModule::GSMModule() {
    #if USE_UART2_FOR_GSM
        gsmSerial = &Serial2; // Using hardware UART2
    #else
        gsmSerial = &Serial1; // Fallback to UART1
    #endif
    
    moduleReady = false;
    networkRegistered = false;
    smsReady = false;
    gprsConnected = false;
    signalStrength = 0;
    smsSentCount = 0;
    smsFailedCount = 0;
    lastSMSTime = 0;
}

bool GSMModule::initialize() {
    if (DEBUG_MODE) {
        Serial.println("Initializing SIM800L module...");
    }
    
    // Initialize serial port
    gsmSerial->begin(GSM_UART_BAUDRATE, GSM_UART_CONFIG, GSM_RX_PIN, GSM_TX_PIN);
    
    // Wait for module to boot up
    delay(3000);
    
    // Initialization sequence
    struct InitCommand {
        const char* command;
        const char* expected;
        const char* description;
    };
    
    InitCommand initCommands[] = {
        {"AT", "OK", "Basic communication test"},
        {"ATE0", "OK", "Disable echo"},
        {"AT+CMEE=2", "OK", "Enable extended error reporting"},
        {"AT+CREG?", "+CREG:", "Check network registration"},
        {"AT+CSQ", "+CSQ:", "Check signal strength"},
        {"AT+COPS?", "+COPS:", "Check network operator"},
        {"AT+CMGF=1", "OK", "Set SMS text mode"},
        {"AT+CSCS=\"GSM\"", "OK", "Set character set"},
        {"AT+CNMI=1,2,0,0,0", "OK", "Configure SMS notifications"}
    };
    
    bool initSuccess = true;
    
    for (const auto& cmd : initCommands) {
        if (DEBUG_MODE) {
            Serial.print("  ");
            Serial.println(cmd.description);
        }
        
        bool success = sendATCommand(cmd.command, cmd.expected, 10000);
        
        if (success) {
            if (DEBUG_MODE) {
                Serial.print("    ✓ ");
                Serial.print(cmd.command);
                Serial.println(" - OK");
            }
            
            // Parse specific responses
            if (strcmp(cmd.command, "AT+CSQ") == 0) {
                String response = gsmSerial->readString();
                parseSignalStrength(response);
            } else if (strcmp(cmd.command, "AT+COPS?") == 0) {
                String response = gsmSerial->readString();
                parseOperator(response);
            } else if (strcmp(cmd.command, "AT+CREG?") == 0) {
                String response = gsmSerial->readString();
                parseNetworkStatus(response);
            }
        } else {
            if (DEBUG_MODE) {
                Serial.print("    ✗ ");
                Serial.print(cmd.command);
                Serial.println(" - Failed");
            }
            initSuccess = false;
        }
        
        delay(1000);
    }
    
    // Final status check
    checkModuleStatus();
    
    if (moduleReady && DEBUG_MODE) {
        Serial.println("SIM800L initialization successful!");
    } else if (DEBUG_MODE) {
        Serial.println("SIM800L initialization failed - some features may not work");
    }
    
    return moduleReady;
}

bool GSMModule::sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout) {
    // Clear buffer
    while (gsmSerial->available()) {
        gsmSerial->read();
    }
    
    // Send command
    gsmSerial->println(command);
    
    // Wait for response
    unsigned long startTime = millis();
    String response;
    
    while (millis() - startTime < timeout) {
        while (gsmSerial->available()) {
            char c = gsmSerial->read();
            response += c;
            
            // Check for expected response
            if (response.indexOf(expectedResponse) != -1) {
                return true;
            }
            
            // Check for error responses
            if (response.indexOf("ERROR") != -1 || response.indexOf("FAIL") != -1) {
                return false;
            }
        }
    }
    
    return false;
}

void GSMModule::parseSignalStrength(const String& response) {
    int start = response.indexOf("+CSQ:");
    if (start != -1) {
        int comma = response.indexOf(',', start);
        if (comma != -1) {
            int rssi = response.substring(start + 6, comma).toInt();
            
            if (rssi == 99) {
                signalStrength = 0; // Unknown
            } else if (rssi >= 20) {
                signalStrength = 5; // Excellent
            } else if (rssi >= 15) {
                signalStrength = 4; // Good
            } else if (rssi >= 10) {
                signalStrength = 3; // Fair
            } else if (rssi >= 5) {
                signalStrength = 2; // Marginal
            } else {
                signalStrength = 1; // Poor
            }
            
            if (DEBUG_MODE) {
                Serial.print("    Signal strength: ");
                Serial.print(signalStrength);
                Serial.print("/5 (RSSI: ");
                Serial.print(rssi);
                Serial.println(")");
            }
        }
    }
}

void GSMModule::parseOperator(const String& response) {
    int quote1 = response.indexOf('"');
    if (quote1 != -1) {
        int quote2 = response.indexOf('"', quote1 + 1);
        if (quote2 != -1) {
            operatorName = response.substring(quote1 + 1, quote2);
            if (DEBUG_MODE) {
                Serial.print("    Network operator: ");
                Serial.println(operatorName);
            }
        }
    }
}

void GSMModule::parseNetworkStatus(const String& response) {
    int start = response.indexOf("+CREG:");
    if (start != -1) {
        int comma = response.indexOf(',', start);
        if (comma != -1) {
            int status = response.substring(comma + 1, comma + 2).toInt();
            networkRegistered = (status == 1 || status == 5);
            
            if (DEBUG_MODE) {
                Serial.print("    Network: ");
                Serial.println(networkRegistered ? "Registered" : "Not registered");
            }
        }
    }
}

bool GSMModule::checkModuleStatus() {
    bool commSuccess = sendATCommand("AT", "OK", 5000);
    
    if (commSuccess && networkRegistered && signalStrength > 0) {
        moduleReady = true;
        smsReady = true;
    } else {
        moduleReady = false;
        smsReady = false;
    }
    
    return moduleReady;
}

bool GSMModule::sendSMS(const String& number, const String& message) {
    if (!smsReady) {
        if (DEBUG_MODE) {
            Serial.println("SMS not ready - checking module status...");
        }
        checkModuleStatus();
        if (!smsReady) {
            return false;
        }
    }
    
    // Rate limiting
    unsigned long currentTime = millis();
    if (currentTime - lastSMSTime < 30000) { // 30 seconds minimum between SMS
        if (DEBUG_MODE) {
            Serial.println("SMS rate limited - waiting...");
        }
        return false;
    }
    
    if (DEBUG_MODE) {
        Serial.print("Sending SMS to ");
        Serial.print(number);
        Serial.println("...");
    }
    
    // Set SMS recipient
    String cmd = "AT+CMGS=\"" + number + "\"";
    bool success = sendATCommand(cmd, ">", 10000);
    
    if (!success) {
        if (DEBUG_MODE) {
            Serial.println("Failed to set SMS recipient");
        }
        smsFailedCount++;
        return false;
    }
    
    // Send message content
    gsmSerial->print(message);
    gsmSerial->write(26); // Ctrl+Z to send
    
    // Wait for confirmation
    unsigned long startTime = millis();
    String response;
    
    while (millis() - startTime < 30000) { // 30 second timeout for SMS
        while (gsmSerial->available()) {
            char c = gsmSerial->read();
            response += c;
            
            if (response.indexOf("+CMGS:") != -1) {
                if (DEBUG_MODE) {
                    Serial.println("  ✓ SMS sent successfully");
                }
                smsSentCount++;
                lastSMSTime = currentTime;
                return true;
            }
            
            if (response.indexOf("ERROR") != -1) {
                if (DEBUG_MODE) {
                    Serial.println("  ✗ SMS failed");
                }
                smsFailedCount++;
                return false;
            }
        }
    }
    
    if (DEBUG_MODE) {
        Serial.println("  ✗ SMS timeout");
    }
    smsFailedCount++;
    return false;
}

bool GSMModule::sendSMSToRecipients(const String message) {
    bool anySuccess = false;
    
    for (int i = 0; i < SMS_RECIPIENT_COUNT; i++) {
        if (sendSMS(SMS_RECIPIENTS[i], message)) {
            anySuccess = true;
        }
        delay(2000); // Delay between messages
    }
    
    return anySuccess;
}

bool GSMModule::sendThresholdAlert(const String& tenant, const String& alertType, float value, float threshold) {
    String timestamp = getTimestamp();
    String message;
    
    if (alertType == "energy") {
        message = "ENERGY ALERT\n";
        message += "Time: " + timestamp + "\n";
        message += "Tenant " + tenant + ": " + String(value, 1) + "kWh\n";
        message += "Limit: " + String(threshold, 1) + "kWh\n";
        message += "Exceeded by: " + String(value - threshold, 1) + "kWh\n";
        message += "Please reduce usage.";
    } else { // cost alert
        message = "COST ALERT\n";
        message += "Time: " + timestamp + "\n";
        message += "Tenant " + tenant + ": ₵" + String(value, 2) + "\n";
        message += "Limit: ₵" + String(threshold, 2) + "\n";
        message += "Exceeded by: ₵" + String(value - threshold, 2) + "\n";
        message += "Please reduce usage.";
    }
    
    return sendSMSToRecipients(message);
}

bool GSMModule::sendDailyReport(float energyA, float costA, float energyB, float costB) {
    String date = getTimestamp().substring(0, 10); // Get just the date part
    
    String message = "DAILY ENERGY REPORT\n";
    message += "Date: " + date + "\n\n";
    message += "TENANT A:\n";
    message += "  Energy: " + String(energyA, 1) + "kWh\n";
    message += "  Cost: ₵" + String(costA, 2) + "\n\n";
    message += "TENANT B:\n";
    message += "  Energy: " + String(energyB, 1) + "kWh\n";
    message += "  Cost: ₵" + String(costB, 2) + "\n\n";
    
    float totalEnergy = energyA + energyB;
    float totalCost = costA + costB;
    
    message += "TOTAL:\n";
    message += "  Energy: " + String(totalEnergy, 1) + "kWh\n";
    message += "  Cost: ₵" + String(totalCost, 2) + "\n\n";
    message += "Monitor: bit.ly/energy-dashboard";
    
    return sendSMSToRecipients(message);
}

bool GSMModule::sendSystemAlert(const String& errorMessage) {
    String timestamp = getTimestamp();
    
    String message = "SYSTEM ALERT\n";
    message += "Time: " + timestamp + "\n";
    message += "Error: " + errorMessage + "\n";
    message += "Check device immediately.";
    
    return sendSMSToRecipients(message);
}

bool GSMModule::setupGPRS(const String& apn) {
    if (DEBUG_MODE) {
        Serial.println("Setting up GPRS connection...");
    }

    // Create APN command string separately
    String apnCommand = "AT+SAPBR=3,1,\"APN\",\"" + apn + "\"";
    
    // Execute commands in sequence with proper error handling
    bool success = true;
    
    // 1. Set connection type
    if (DEBUG_MODE) Serial.println("  Setting connection type...");
    if (!sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 15000)) {
        if (DEBUG_MODE) Serial.println("    ✗ Set connection type - Failed");
        success = false;
    } else if (DEBUG_MODE) {
        Serial.println("    ✓ Set connection type - OK");
    }
    delay(2000);

    // 2. Set APN
    if (DEBUG_MODE) Serial.println("  Setting APN...");
    if (!sendATCommand(apnCommand, "OK", 15000)) {
        if (DEBUG_MODE) Serial.println("    ✗ Set APN - Failed");
        success = false;
    } else if (DEBUG_MODE) {
        Serial.println("    ✓ Set APN - OK");
    }
    delay(2000);

    // 3. Open GPRS context
    if (DEBUG_MODE) Serial.println("  Opening GPRS context...");
    if (!sendATCommand("AT+SAPBR=1,1", "OK", 15000)) {
        if (DEBUG_MODE) Serial.println("    ✗ Open GPRS context - Failed");
        success = false;
    } else if (DEBUG_MODE) {
        Serial.println("    ✓ Open GPRS context - OK");
    }
    delay(2000);

    // 4. Check GPRS status
    if (DEBUG_MODE) Serial.println("  Checking GPRS status...");
    if (!sendATCommand("AT+SAPBR=2,1", "+SAPBR: 1,1", 15000)) {
        if (DEBUG_MODE) Serial.println("    ✗ Check GPRS status - Failed");
        success = false;
    } else if (DEBUG_MODE) {
        Serial.println("    ✓ Check GPRS status - OK");
    }

    if (success) {
        gprsConnected = true;
        if (DEBUG_MODE) Serial.println("GPRS connection established!");
    }
    
    return success;
}

bool GSMModule::sendHTTPRequest(const String& url, const String& data) {
    if (!gprsConnected) {
        if (!setupGPRS()) {
            return false;
        }
    }
    
    // Initialize HTTP service
    if (!sendATCommand("AT+HTTPINIT", "OK", 10000)) {
        return false;
    }
    
    // Set HTTP parameters
    sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 5000);
    sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 5000);
    
    if (data.length() > 0) {
        // POST request with data
        sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 5000);
        sendATCommand("AT+HTTPDATA=" + String(data.length()) + ",10000", "DOWNLOAD", 10000);
        
        // Send data
        gsmSerial->print(data);
        delay(2000);
        
        // Execute POST
        if (!sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 30000)) {
            sendATCommand("AT+HTTPTERM", "OK", 5000);
            return false;
        }
    } else {
        // GET request
        if (!sendATCommand("AT+HTTPACTION=0", "+HTTPACTION: 0,200", 30000)) {
            sendATCommand("AT+HTTPTERM", "OK", 5000);
            return false;
        }
    }
    
    // Terminate HTTP service
    sendATCommand("AT+HTTPTERM", "OK", 5000);
    return true;
}

GSMModule::ModuleStatus GSMModule::getStatus() {
    ModuleStatus status;
    status.moduleReady = moduleReady;
    status.networkRegistered = networkRegistered;
    status.smsReady = smsReady;
    status.gprsConnected = gprsConnected;
    status.signalStrength = signalStrength;
    status.operatorName = operatorName;
    status.smsSentCount = smsSentCount;
    status.smsFailedCount = smsFailedCount;
    return status;
}

String GSMModule::getTimestamp() {
    // This should be replaced with actual RTC time if available
    // For now, just return a formatted string of millis()
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02lu/%02lu/%04lu %02lu:%02lu", 
             days % 31 + 1, (days / 31) % 12 + 1, 2023 + days / 365,
             hours % 24, minutes % 60);
    
    return String(buffer);
}