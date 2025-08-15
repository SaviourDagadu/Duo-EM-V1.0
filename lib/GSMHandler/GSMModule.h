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
    
    // GPRS/Data Functions
    bool setupGPRS(const String& apn = "internet");
    bool sendHTTPRequest(const String& url, const String& data = "");
    
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
    };
    
    ModuleStatus getStatus();
    
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
    unsigned long lastSMSTime;
    
    // Helper functions
    bool sendATCommand(const String& command, const String& expectedResponse = "OK", unsigned long timeout = 10000);
    void parseSignalStrength(const String& response);
    void parseOperator(const String& response);
    void parseNetworkStatus(const String& response);
    bool checkModuleStatus();
    String getTimestamp();
};

#endif // GSMMODULE_H