#ifndef LCDINTERFACE_H
#define LCDINTERFACE_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "SensorHandler.h"

class LCDInterface {
public:
    LCDInterface();
    void begin();
    void updateDisplay(const PZEMResult& energyData, const StatusResult& status);
    void nextPage();
    void showSystemMessage(const String& message, unsigned long duration = 2000);
    void showAlert(const String& message);

private:
    LiquidCrystal_I2C lcd;
    unsigned long lastUpdateTime;
    unsigned long pageChangeTime;
    int currentPage;
    bool showingMessage;
    unsigned long messageEndTime;
    String currentMessage;
    
    void displayPage1(const PZEMReading& data);
    void displayPage2(const PZEMReading& data);
    void displayPage3(const PZEMResult& data);
    void displayPage4(const StatusResult& status);
    void clearLine(int line);
    String formatFloat(float value, int precision);
};

#endif