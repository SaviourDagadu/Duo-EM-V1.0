#include "GSMModule.h"
#include "config.h"

GSMModule::GSMModule() {
    #if USE_UART2_FOR_GSM
        gsmSerial = &Serial2;
    #else
        gsmSerial = &Serial1;
    #endif
    
    moduleReady = false;
    networkRegistered = false;
    smsReady = false;
    gprsConnected = false;
    signalStrength = 0;
    smsSentCount = 0;
    smsFailedCount = 0;
    smsReceivedCount = 0;
    lastSMSTime = 0;
    moduleStartTime = 0;
    bufferedCount = 0;
    lastSMSIndex = -1;
    lastError = "";
}

bool GSMModule::initialize() {
    moduleStartTime = millis();
    
    if (DEBUG_MODE) {
        Serial.println("Initializing SIM800L module...");
    }
    
    gsmSerial->begin(GSM_UART_BAUDRATE, GSM_UART_CONFIG, GSM_RX_PIN, GSM_TX_PIN);
    delay(3000);
    
    clearSerialBuffer();
    
    struct InitCommand {
        const char* command;
        const char* expected;
        const char* description;
        bool critical;
    };
    
    InitCommand initCommands[] = {
        {"AT", "OK", "Basic communication test", true},
        {"ATE0", "OK", "Disable echo", true},
        {"AT+CMEE=2", "OK", "Enable extended error reporting", false},
        {"AT+CREG?", "+CREG:", "Check network registration", true},
        {"AT+CSQ", "+CSQ:", "Check signal strength", true},
        {"AT+COPS?", "+COPS:", "Check network operator", false},
        {"AT+CMGF=1", "OK", "Set SMS text mode", true},
        {"AT+CSCS=\"GSM\"", "OK", "Set character set", false},
        {"AT+CNMI=1,2,0,0,0", "OK", "Configure SMS notifications", true},
        {"AT+CPMS=\"SM\",\"SM\",\"SM\"", "OK", "Set SMS storage to SIM", false}
    };
    
    bool initSuccess = true;
    int criticalFailures = 0;
    
    for (const auto& cmd : initCommands) {
        if (DEBUG_MODE) {
            Serial.print("  ");
            Serial.println(cmd.description);
        }
        
        bool success = sendATCommand(cmd.command, cmd.expected, 15000);
        
        if (success) {
            if (DEBUG_MODE) {
                Serial.print("    ✓ ");
                Serial.print(cmd.command);
                Serial.println(" - OK");
            }
            
            if (strcmp(cmd.command, "AT+CSQ") == 0) {
                String response = sendATCommandWithResponse("AT+CSQ", 5000);
                parseSignalStrength(response);
            } else if (strcmp(cmd.command, "AT+COPS?") == 0) {
                String response = sendATCommandWithResponse("AT+COPS?", 10000);
                parseOperator(response);
            } else if (strcmp(cmd.command, "AT+CREG?") == 0) {
                String response = sendATCommandWithResponse("AT+CREG?", 10000);
                parseNetworkStatus(response);
            }
        } else {
            if (DEBUG_MODE) {
                Serial.print("    ✗ ");
                Serial.print(cmd.command);
                Serial.println(" - Failed");
            }
            
            if (cmd.critical) {
                criticalFailures++;
                logError("Critical init failed: " + String(cmd.command));
            }
            initSuccess = false;
        }
        
        delay(1000);
    }
    
    checkModuleStatus();
    
    if (moduleReady && criticalFailures == 0) {
        if (DEBUG_MODE) {
            Serial.println("✓ SIM800L initialization successful!");
            printDetailedStatus();
        }
    } else {
        if (DEBUG_MODE) {
            Serial.print("✗ SIM800L initialization failed - ");
            Serial.print(criticalFailures);
            Serial.println(" critical failures");
        }
    }
    
    return moduleReady;
}

bool GSMModule::sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout) {
    clearSerialBuffer();
    
    gsmSerial->println(command);
    
    unsigned long startTime = millis();
    String response;
    
    while (millis() - startTime < timeout) {
        while (gsmSerial->available()) {
            char c = gsmSerial->read();
            response += c;
            
            if (response.indexOf(expectedResponse) != -1) {
                return true;
            }
            
            if (response.indexOf("ERROR") != -1 || response.indexOf("FAIL") != -1) {
                lastError = "AT command failed: " + command;
                return false;
            }
        }
        delay(10);
    }
    
    lastError = "AT command timeout: " + command;
    return false;
}

String GSMModule::sendATCommandWithResponse(const String& command, unsigned long timeout) {
    clearSerialBuffer();
    
    gsmSerial->println(command);
    
    unsigned long startTime = millis();
    String response;
    
    while (millis() - startTime < timeout) {
        while (gsmSerial->available()) {
            char c = gsmSerial->read();
            response += c;
        }
        
        if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
            break;
        }
        delay(10);
    }
    
    return response;
}

// NEW: Enhanced SMS Receiving Functions
bool GSMModule::checkIncomingSMS() {
    if (!smsReady) return false;
    
    String response = sendATCommandWithResponse("AT+CMGL=\"ALL\"", 10000);
    
    if (response.indexOf("+CMGL:") != -1) {
        String smsContent = extractSMSContent(response);
        String smsSender = extractSMSSender(response);
        
        if (smsContent.length() > 0) {
            lastSMSMessage = smsContent;
            lastSMSSender = smsSender;
            smsReceivedCount++;
            
            if (DEBUG_MODE) {
                Serial.println("📱 New SMS received:");
                Serial.println("  From: " + smsSender);
                Serial.println("  Message: " + smsContent);
            }
            
            return true;
        }
    }
    
    return false;
}

String GSMModule::parseIncomingSMS() {
    if (!checkIncomingSMS()) {
        return "";
    }
    
    if (isAuthorizedNumber(lastSMSSender)) {
        SMSCommand cmd = parseSMSCommand(lastSMSMessage, lastSMSSender);
        
        if (cmd.isValid) {
            processSMSCommand(cmd);
            deleteAllSMS(); // Clean up after processing
            return "Command processed: " + cmd.command;
        } else {
            sendSMS(lastSMSSender, "Invalid command. Send 'HELP' for available commands.");
            return "Invalid command from: " + lastSMSSender;
        }
    } else {
        if (DEBUG_MODE) {
            Serial.println("Unauthorized SMS sender: " + lastSMSSender);
        }
        return "Unauthorized sender";
    }
}

GSMModule::SMSCommand GSMModule::parseSMSCommand(const String& message, const String& sender) {
    SMSCommand cmd;
    cmd.sender = sender;
    cmd.isValid = false;
    
    String cleanMessage = message;
    cleanMessage.trim();
    cleanMessage.toUpperCase();
    
    int spaceIndex = cleanMessage.indexOf(' ');
    
    if (spaceIndex != -1) {
        cmd.command = cleanMessage.substring(0, spaceIndex);
        cmd.parameter = cleanMessage.substring(spaceIndex + 1);
    } else {
        cmd.command = cleanMessage;
        cmd.parameter = "";
    }
    
    cmd.isValid = isValidSMSCommand(cmd.command);
    
    return cmd;
}

bool GSMModule::processSMSCommand(const SMSCommand& cmd) {
    String response = "";
    bool success = false;
    
    if (cmd.command == "STATUS") {
        response = generateStatusResponse();
        success = true;
    }
    else if (cmd.command == "REPORT") {
        // This would need integration with sensor data
        response = "Daily report feature requires sensor integration";
        success = true;
    }
    else if (cmd.command == "HELP") {
        response = generateHelpResponse();
        success = true;
    }
    else if (cmd.command == "SIGNAL") {
        response = "Signal: " + getSignalQualityDescription();
        success = true;
    }
    else if (cmd.command == "RESET") {
        if (cmd.parameter == "COUNTERS") {
            resetCounters();
            response = "Counters reset successfully";
        } else {
            response = "Reset requires parameter: COUNTERS";
        }
        success = true;
    }
    else if (cmd.command == "SET") {
        response = "SET commands not yet implemented";
        success = true;
    }
    
    if (success) {
        return sendSMS(cmd.sender, response);
    }
    
    return false;
}

String GSMModule::generateStatusResponse() {
    String status = "SYSTEM STATUS\\n";
    status += "Network: " + getNetworkStatusDescription() + "\\n";
    status += "Signal: " + getSignalQualityDescription() + "\\n";
    status += "SMS Sent: " + String(smsSentCount) + "\\n";
    status += "SMS Received: " + String(smsReceivedCount) + "\\n";
    status += "GPRS: " + String(gprsConnected ? "Connected" : "Disconnected") + "\\n";
    status += "Uptime: " + String((millis() - moduleStartTime) / 60000) + " min";
    
    return status;
}

String GSMModule::generateHelpResponse() {
    String help = "AVAILABLE COMMANDS:\\n";
    help += "STATUS - System status\\n";
    help += "REPORT - Daily report\\n";
    help += "SIGNAL - Signal strength\\n";
    help += "RESET COUNTERS - Reset stats\\n";
    help += "HELP - This message";
    
    return help;
}

// Enhanced GPRS Functions
bool GSMModule::setupGPRS(const String& apn) {
    if (DEBUG_MODE) {
        Serial.println("Setting up GPRS connection...");
    }

    String apnCommand = "AT+SAPBR=3,1,\"APN\",\"" + apn + "\"";
    
    bool success = true;
    
    struct GPRSCommand {
        String command;
        String expected;
        String description;
    };
    
    GPRSCommand gprsCommands[] = {
        {"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", "Set connection type"},
        {apnCommand, "OK", "Set APN"},
        {"AT+SAPBR=1,1", "OK", "Open GPRS context"},
        {"AT+SAPBR=2,1", "+SAPBR: 1,1", "Check GPRS status"}
    };
    
    for (const auto& cmd : gprsCommands) {
        if (DEBUG_MODE) {
            Serial.println("  " + cmd.description + "...");
        }
        
        if (sendATCommand(cmd.command, cmd.expected, 20000)) {
            if (DEBUG_MODE) {
                Serial.println("    ✓ " + cmd.description + " - OK");
            }
        } else {
            if (DEBUG_MODE) {
                Serial.println("    ✗ " + cmd.description + " - Failed");
            }
            success = false;
            break;
        }
        delay(2000);
    }
    
    if (success) {
        gprsConnected = true;
        String response = sendATCommandWithResponse("AT+SAPBR=2,1", 5000);
        // Extract IP address from response if available
        
        if (DEBUG_MODE) {
            Serial.println("✓ GPRS connection established!");
        }
    }
    
    return success;
}

bool GSMModule::sendDataWithRetry(const String& url, const String& data, int maxRetries) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        if (DEBUG_MODE) {
            Serial.print("📡 HTTP attempt ");
            Serial.print(attempt);
            Serial.print("/");
            Serial.println(maxRetries);
        }
        
        if (!gprsConnected) {
            if (!setupGPRS()) {
                continue;
            }
        }
        
        if (sendHTTPRequest(url, data)) {
            if (DEBUG_MODE) {
                Serial.println("✓ HTTP request successful");
            }
            return true;
        }
        
        if (attempt < maxRetries) {
            if (DEBUG_MODE) {
                Serial.println("HTTP failed, retrying in 5 seconds...");
            }
            delay(5000);
        }
    }
    
    if (DEBUG_MODE) {
        Serial.println("✗ HTTP failed after all retries, buffering data");
    }
    bufferDataForLater(data);
    return false;
}

bool GSMModule::bufferDataForLater(const String& data) {
    if (bufferedCount < MAX_BUFFERED_ENTRIES) {
        bufferedData[bufferedCount] = data;
        bufferedCount++;
        
        if (DEBUG_MODE) {
            Serial.print("Data buffered (");
            Serial.print(bufferedCount);
            Serial.print("/");
            Serial.print(MAX_BUFFERED_ENTRIES);
            Serial.println(")");
        }
        return true;
    }
    
    if (DEBUG_MODE) {
        Serial.println("Buffer full, discarding oldest data");
    }
    
    for (int i = 0; i < MAX_BUFFERED_ENTRIES - 1; i++) {
        bufferedData[i] = bufferedData[i + 1];
    }
    bufferedData[MAX_BUFFERED_ENTRIES - 1] = data;
    
    return true;
}

bool GSMModule::sendBufferedData() {
    if (bufferedCount == 0) return true;
    
    if (!gprsConnected) {
        if (!setupGPRS()) {
            return false;
        }
    }
    
    if (DEBUG_MODE) {
        Serial.print("Sending ");
        Serial.print(bufferedCount);
        Serial.println(" buffered entries");
    }
    
    int sent = 0;
    for (int i = 0; i < bufferedCount; i++) {
        if (sendHTTPRequest(bufferedData[i])) {
            sent++;
        } else {
            break;
        }
        delay(1000);
    }
    
    if (sent > 0) {
        for (int i = sent; i < bufferedCount; i++) {
            bufferedData[i - sent] = bufferedData[i];
        }
        bufferedCount -= sent;
        
        if (DEBUG_MODE) {
            Serial.print("✓ Sent ");
            Serial.print(sent);
            Serial.println(" buffered entries");
        }
    }
    
    return (sent > 0);
}

// Enhanced SMS Functions
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
    
    unsigned long currentTime = millis();
    if (currentTime - lastSMSTime < 10000) {
        if (DEBUG_MODE) {
            Serial.println("SMS rate limited - waiting...");
        }
        delay(10000 - (currentTime - lastSMSTime));
    }
    
    if (DEBUG_MODE) {
        Serial.print("Sending SMS to ");
        Serial.print(number);
        Serial.println("...");
    }
    
    clearSerialBuffer();
    
    String cmd = "AT+CMGS=\"" + number + "\"";
    bool success = sendATCommand(cmd, ">", 15000);
    
    if (!success) {
        if (DEBUG_MODE) {
            Serial.println("✗ Failed to set SMS recipient");
        }
        smsFailedCount++;
        return false;
    }
    
    gsmSerial->print(message);
    gsmSerial->write(26);
    
    unsigned long startTime = millis();
    String response;
    
    while (millis() - startTime < 30000) {
        while (gsmSerial->available()) {
            char c = gsmSerial->read();
            response += c;
            
            if (response.indexOf("+CMGS:") != -1) {
                if (DEBUG_MODE) {
                    Serial.println("✓ SMS sent successfully");
                }
                smsSentCount++;
                lastSMSTime = millis();
                return true;
            }
            
            if (response.indexOf("ERROR") != -1) {
                if (DEBUG_MODE) {
                    Serial.println("✗ SMS failed with error");
                }
                smsFailedCount++;
                return false;
            }
        }
        delay(100);
    }
    
    if (DEBUG_MODE) {
        Serial.println("✗ SMS timeout");
    }
    smsFailedCount++;
    return false;
}

bool GSMModule::sendHTTPRequest(const String& url, const String& data) {
    if (!gprsConnected) {
        if (!setupGPRS()) {
            return false;
        }
    }
    
    if (!sendATCommand("AT+HTTPINIT", "OK", 10000)) {
        return false;
    }
    
    sendATCommand("AT+HTTPPARA=\"CID\",1", "OK", 5000);
    
    String urlCmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    if (!sendATCommand(urlCmd, "OK", 10000)) {
        sendATCommand("AT+HTTPTERM", "OK", 5000);
        return false;
    }
    
    bool success = false;
    
    if (data.length() > 0) {
        sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 5000);
        String dataCmd = "AT+HTTPDATA=" + String(data.length()) + ",10000";
        
        if (sendATCommand(dataCmd, "DOWNLOAD", 15000)) {
            gsmSerial->print(data);
            delay(2000);
            
            success = sendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1,200", 30000);
        }
    } else {
        success = sendATCommand("AT+HTTPACTION=0", "+HTTPACTION: 0,200", 30000);
    }
    
    sendATCommand("AT+HTTPTERM", "OK", 5000);
    return success;
}

// Diagnostic Functions
bool GSMModule::runFullDiagnostics() {
    if (DEBUG_MODE) {
        Serial.println("\\nRUNNING FULL GSM DIAGNOSTICS");
        Serial.println("================================");
    }
    
    bool allPassed = true;
    
    // Test 1: Basic Communication
    if (DEBUG_MODE) Serial.print("1. Basic AT Communication... ");
    if (sendATCommand("AT", "OK", 5000)) {
        if (DEBUG_MODE) Serial.println("✓ PASS");
    } else {
        if (DEBUG_MODE) Serial.println("✗ FAIL");
        allPassed = false;
    }
    
    // Test 2: Network Registration
    if (DEBUG_MODE) Serial.print("2. Network Registration... ");
    String regResponse = sendATCommandWithResponse("AT+CREG?", 5000);
    parseNetworkStatus(regResponse);
    if (networkRegistered) {
        if (DEBUG_MODE) Serial.println("✓ PASS");
    } else {
        if (DEBUG_MODE) Serial.println("✗ FAIL");
        allPassed = false;
    }
    
    // Test 3: Signal Strength
    if (DEBUG_MODE) Serial.print("3. Signal Strength... ");
    String sigResponse = sendATCommandWithResponse("AT+CSQ", 5000);
    parseSignalStrength(sigResponse);
    if (signalStrength > 0) {
        if (DEBUG_MODE) {
            Serial.print("✓ PASS (");
            Serial.print(signalStrength);
            Serial.println("/5)");
        }
    } else {
        if (DEBUG_MODE) Serial.println("✗ FAIL");
        allPassed = false;
    }
    
    // Test 4: SMS Ready
    if (DEBUG_MODE) Serial.print("4. SMS Functionality... ");
    if (sendATCommand("AT+CMGF=1", "OK", 5000)) {
        smsReady = true;
        if (DEBUG_MODE) Serial.println("✓ PASS");
    } else {
        if (DEBUG_MODE) Serial.println("✗ FAIL");
        smsReady = false;
        allPassed = false;
    }
    
    // Test 5: GPRS Test
    if (DEBUG_MODE) Serial.print("5. GPRS Connectivity... ");
    if (testGPRSConnectivity()) {
        if (DEBUG_MODE) Serial.println("✓ PASS");
    } else {
        if (DEBUG_MODE) Serial.println("✗ FAIL");
        allPassed = false;
    }
    
    if (DEBUG_MODE) {
        Serial.println("================================");
        Serial.print("DIAGNOSTICS ");
        Serial.println(allPassed ? "PASSED" : "FAILED");
        Serial.println();
    }
    
    return allPassed;
}

bool GSMModule::testGPRSConnectivity() {
    return setupGPRS();
}

bool GSMModule::testHTTPRequest() {
    String testUrl = "http://httpbin.org/get";
    return sendHTTPRequest(testUrl);
}

void GSMModule::printDetailedStatus() {
    Serial.println("\\nGSM MODULE STATUS:");
    Serial.println("=====================");
    Serial.println("Module Ready: " + String(moduleReady ? "YES" : "NO"));
    Serial.println("Network: " + getNetworkStatusDescription());
    Serial.println("Signal: " + getSignalQualityDescription());
    Serial.println("Operator: " + (operatorName.length() > 0 ? operatorName : "Unknown"));
    Serial.println("SMS Ready: " + String(smsReady ? "YES" : "NO"));
    Serial.println("GPRS Connected: " + String(gprsConnected ? "YES" : "NO"));
    Serial.println("SMS Sent: " + String(smsSentCount));
    Serial.println("SMS Failed: " + String(smsFailedCount));
    Serial.println("SMS Received: " + String(smsReceivedCount));
    Serial.println("Uptime: " + String((millis() - moduleStartTime) / 1000) + " seconds");
    if (lastError.length() > 0) {
        Serial.println("Last Error: " + lastError);
    }
    Serial.println("=====================\\n");
}

// Helper Functions
void GSMModule::clearSerialBuffer() {
    while (gsmSerial->available()) {
        gsmSerial->read();
    }
}

String GSMModule::getSignalQualityDescription() {
    switch (signalStrength) {
        case 5: return "Excellent";
        case 4: return "Good";
        case 3: return "Fair";
        case 2: return "Marginal";
        case 1: return "Poor";
        default: return "No Signal";
    }
}

String GSMModule::getNetworkStatusDescription() {
    return networkRegistered ? "Registered" : "Not Registered";
}

bool GSMModule::isAuthorizedNumber(const String& number) {
    // Check against authorized numbers from config
    for (int i = 0; i < SMS_RECIPIENT_COUNT; i++) {
        if (number == SMS_RECIPIENTS[i]) {
            return true;
        }
    }
    return false;
}

bool GSMModule::isValidSMSCommand(const String& command) {
    String validCommands[] = {"STATUS", "REPORT", "HELP", "SIGNAL", "RESET", "SET"};
    int commandCount = sizeof(validCommands) / sizeof(validCommands[0]);
    
    for (int i = 0; i < commandCount; i++) {
        if (command == validCommands[i]) {
            return true;
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
                signalStrength = 0;
            } else if (rssi >= 20) {
                signalStrength = 5;
            } else if (rssi >= 15) {
                signalStrength = 4;
            } else if (rssi >= 10) {
                signalStrength = 3;
            } else if (rssi >= 5) {
                signalStrength = 2;
            } else {
                signalStrength = 1;
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

String GSMModule::extractSMSContent(const String& response) {
    int contentStart = response.lastIndexOf('\n');
    if (contentStart != -1) {
        String content = response.substring(contentStart + 1);
        content.trim();
        return content;
    }
    return "";
}

String GSMModule::extractSMSSender(const String& response) {
    int quote1 = response.indexOf('"');
    if (quote1 != -1) {
        int quote2 = response.indexOf('"', quote1 + 1);
        if (quote2 != -1) {
            return response.substring(quote1 + 1, quote2);
        }
    }
    return "";
}

bool GSMModule::deleteAllSMS() {
    return sendATCommand("AT+CMGDA=\"DEL ALL\"", "OK", 10000);
}

bool GSMModule::deleteSMS(int index) {
    String cmd = "AT+CMGD=" + String(index);
    return sendATCommand(cmd, "OK", 5000);
}

String GSMModule::getLastSMSMessage() {
    return lastSMSMessage;
}

String GSMModule::getLastSMSSender() {
    return lastSMSSender;
}

void GSMModule::resetCounters() {
    smsSentCount = 0;
    smsFailedCount = 0;
    smsReceivedCount = 0;
    bufferedCount = 0;
    moduleStartTime = millis();
}

void GSMModule::logError(const String& error) {
    lastError = error;
    if (DEBUG_MODE) {
        Serial.println("⚠️ GSM Error: " + error);
    }
}

bool GSMModule::reconnectGPRS() {
    if (DEBUG_MODE) {
        Serial.println("🔄 Reconnecting GPRS...");
    }
    
    sendATCommand("AT+SAPBR=0,1", "OK", 5000);
    delay(2000);
    
    return setupGPRS();
}

bool GSMModule::isGPRSConnected() {
    String response = sendATCommandWithResponse("AT+SAPBR=2,1", 5000);
    return response.indexOf("+SAPBR: 1,1") != -1;
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
    status.smsReceivedCount = smsReceivedCount;
    status.lastError = lastError;
    status.uptime = millis() - moduleStartTime;
    status.ipAddress = ipAddress;
    return status;
}

bool GSMModule::sendSMSToRecipients(const String message) {
    bool anySuccess = false;
    
    for (int i = 0; i < SMS_RECIPIENT_COUNT; i++) {
        if (sendSMS(SMS_RECIPIENTS[i], message)) {
            anySuccess = true;
        }
        delay(3000);
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
    } else {
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
    String date = getTimestamp().substring(0, 10);
    
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

String GSMModule::getTimestamp() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02lu/%02lu/%04lu %02lu:%02lu", 
             days % 31 + 1, (days / 31) % 12 + 1, 2024 + days / 365,
             hours % 24, minutes % 60);
    
    return String(buffer);
}

bool GSMModule::testSMSFunctionality(const String& testNumber) {
    if (DEBUG_MODE) {
        Serial.println("📱 Testing SMS functionality...");
    }
    
    String testMessage = "SMS Test - " + getTimestamp();
    return sendSMS(testNumber, testMessage);
}

bool GSMModule::enterSleepMode() {
    return sendATCommand("AT+CSCLK=1", "OK", 5000);
}

bool GSMModule::wakeFromSleep() {
    gsmSerial->println("AT");
    delay(1000);
    return sendATCommand("AT+CSCLK=0", "OK", 5000);
}

bool GSMModule::setPowerSaveMode(bool enable) {
    String cmd = enable ? "AT+CFUN=0" : "AT+CFUN=1";
    return sendATCommand(cmd, "OK", 10000);
}