#ifndef ALERTHANDLER_H
#define ALERTHANDLER_H

#include <Arduino.h>
#include "config.h"

class AlertHandler {
public:
    AlertHandler();
    void begin();
    
    // Status indicators
    void setSystemStatus(bool normalOperation);
    void setCommunicationStatus(bool active);
    
    // Threshold alerts
    void triggerEnergyAlert(uint8_t tenant); // 1 = Tenant A, 2 = Tenant B, 3 = Both
    void clearEnergyAlert();
    
    // System alerts
    void triggerSystemAlert();
    void clearSystemAlert();
    
    // Update function to be called in main loop
    void update();

private:
    unsigned long lastBlinkTime;
    bool alertActive;
    bool systemAlertActive;
    bool communicationActive;
    
    void updateLEDs();
    void updateBuzzer();
};

#endif // ALERTHANDLER_H