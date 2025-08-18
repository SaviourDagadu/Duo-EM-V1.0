#ifndef GSMMODULE_H
#define GSMMODULE_H

#include <HardwareSerial.h>
#include <Arduino.h>
#include "config.h"

class GSMModule {
public:
    GSMModule();
    bool initialize();
    
    // SMS Functions
    bool sendSMS(const String& number, const String& message);
    bool sendSMSToRecipients(const String message);
    bool sendThresholdAlert(const String& tenant, const String& alertType, float value, float threshold);
    bool sendDailyReport(float energyA, float costA, float energyB, float costB);
    bool sendSystemAlert(const String& errorMessage);
    
    // SMS Receiving Functions
    bool checkIncomingSMS();
    String getLastSMSMessage();
    String getLastSMSSender();
    String parseIncomingSMS();
    bool deleteSMS(int index);
    bool deleteAllSMS();
    
    // SMS Command Processing
    struct SMSCommand {
        String sender;
        String command;
        String parameter;
        bool isValid;
    };
    SMSCommand parseSMSCommand(const String& message, const String& sender);
    bool processSMSCommand(const SMSCommand& cmd);
    String generateStatusResponse();
    String generateHelpResponse();
    
    // GPRS/Data Functions
    bool setupGPRS(const String& apn = "internet");
    bool sendHTTPRequest(const String& url, const String& data = "");
    bool reconnectGPRS();
    bool isGPRSConnected();
    
    // NEW: Enhanced Data Functions
    bool sendDataWithRetry(const String& url, const String& data = "", int maxRetries = 3);
    bool bufferDataForLater(const String& data);
    bool sendBufferedData();
    
    // Status Functions
    struct ModuleStatus {
        bool moduleReady;
        bool networkRegistered;
        bool smsReady;
        bool gprsConnected;
        int signalStrength;
        String operatorName;
        int smsSentCount;
        int smsFailedCount;
        int smsReceivedCount;
        String lastError;
        unsigned long uptime;
        String ipAddress;
    };
    
    ModuleStatus getStatus();
    void resetCounters();
    
    // NEW: Diagnostic Functions
    bool runFullDiagnostics();
    bool testSMSFunctionality(const String& testNumber);
    bool testGPRSConnectivity();
    bool testHTTPRequest();
    void printDetailedStatus();
    String getSignalQualityDescription();
    String getNetworkStatusDescription();
    String getTimestamp();
    
    // NEW: Power Management
    bool enterSleepMode();
    bool wakeFromSleep();
    bool setPowerSaveMode(bool enable);
    
private:
    HardwareSerial* gsmSerial;
    bool moduleReady;
    bool networkRegistered;
    bool smsReady;
    bool gprsConnected;
    int signalStrength;
    String operatorName;
    int smsSentCount;
    int smsFailedCount;
    int smsReceivedCount;
    unsigned long lastSMSTime;
    unsigned long moduleStartTime;
    String lastError;
    String ipAddress;
    
    // NEW: SMS receive variables
    String lastSMSMessage;
    String lastSMSSender;
    int lastSMSIndex;
    
    // NEW: Data buffering
    static const int MAX_BUFFERED_ENTRIES = 10;
    String bufferedData[MAX_BUFFERED_ENTRIES];
    int bufferedCount;
    
    // Helper functions
    bool sendATCommand(const String& command, const String& expectedResponse = "OK", unsigned long timeout = 10000);
    String sendATCommandWithResponse(const String& command, unsigned long timeout = 10000);
    void parseSignalStrength(const String& response);
    void parseOperator(const String& response);
    void parseNetworkStatus(const String& response);
    bool checkModuleStatus();
    
    // NEW: Enhanced helper functions
    void clearSerialBuffer();
    bool waitForResponse(const String& expected, unsigned long timeout);
    String extractQuotedString(const String& input);
    bool isAuthorizedNumber(const String& number);
    void logError(const String& error);
    
    // NEW: SMS parsing helpers
    String extractSMSContent(const String& response);
    String extractSMSSender(const String& response);
    int findNextSMSIndex();
    bool isValidSMSCommand(const String& command);
};

#endif // GSMMODULE_H