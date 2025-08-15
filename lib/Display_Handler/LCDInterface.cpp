#include "LCDInterface.h"

LCDInterface::LCDInterface() : 
    lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS),
    currentPage(0),
    showingMessage(false) {}

void LCDInterface::begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    lcd.init();
    lcd.backlight();
    lcd.clear();

    lastUpdateTime = 0;
    pageChangeTime = millis();
}

String LCDInterface::formatFloat(float value, int precision) {
    String result = String(value, precision);
    // Remove trailing zeros and possible . if no decimals left
    if (result.indexOf('.') != -1) {
        result.replace("0", " ");
        result.trim();
        result.replace(" ", "0");
        if (result.endsWith(".")) {
            result.remove(result.length() - 1);
        }
    }
    return result;
}

void LCDInterface::updateDisplay(const PZEMResult& energyData, const StatusResult& status) {
    unsigned long currentTime = millis();
    
    // Check if we're showing a temporary message
    if (showingMessage) {
        if (currentTime >= messageEndTime) {
            showingMessage = false;
            pageChangeTime = currentTime; // Reset page timer
            lcd.clear();
        } else {
            return; // Don't update display while showing message
        }
    }
    
    // Check if it's time to update the display values
    if (currentTime - lastUpdateTime < DISPLAY_UPDATE_INTERVAL) {
        return;
    }
    
    // Check if it's time to change pages (automatic rotation)
    if (currentTime - pageChangeTime > DISPLAY_PAGE_DURATION) {
        nextPage();
        pageChangeTime = currentTime;
    }
    
    // Display the current page
    switch (currentPage) {
        case 0: displayPage1(energyData.tenant_a); break;
        case 1: displayPage2(energyData.tenant_b); break;
        case 2: displayPage3(energyData); break;
        case 3: displayPage4(status); break;
    }
    
    lastUpdateTime = currentTime;
}

void LCDInterface::nextPage() {
    currentPage = (currentPage + 1) % 4; // Cycle through 4 pages
    lcd.clear();
}

void LCDInterface::showSystemMessage(const String& message, unsigned long duration) {
    showingMessage = true;
    currentMessage = message;
    messageEndTime = millis() + duration;
    
    lcd.clear();
    lcd.setCursor(0, 1);

    // Split long messages across two lines
    if (message.length() > 16) {
        lcd.setCursor(0, 2);
        lcd.print(message.substring(0, 16));
        lcd.setCursor(0, 3);
        lcd.print(message.substring(16));
    } else {
        lcd.setCursor(0, 2);
        lcd.print(message);
    }
}

void LCDInterface::showAlert(const String& message) {
    showSystemMessage(message, 5000); // Alerts show for 5 seconds
    
    // Make the alert more visible
    for (int i = 0; i < 3; i++) {
        lcd.noBacklight();
        delay(200);
        lcd.backlight();
        delay(200);
    }
}

void LCDInterface::displayPage1(const PZEMReading& data) {
    // Clear the display first (optional, can be done before calling this function)
    lcd.clear();
    
    // Line 0: Header (centered)
    lcd.setCursor(2, 0);  // Center "TENANT A" on 16-char display
    lcd.print("TENANT A");
    
    // Line 1: Voltage and Current
    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(formatFloat(data.voltage, 1));
    lcd.print("V");
    
    // Move to right half of line 1
    lcd.setCursor(9, 1);
    lcd.print("I:");
    lcd.print(formatFloat(data.current, 2));
    lcd.print("A");
    
    // Line 2: Power and Power Factor
    lcd.setCursor(0, 2);
    lcd.print("P:");
    lcd.print(formatFloat(data.power, 1));
    lcd.print("W");
    
    // Move to right half of line 2
    lcd.setCursor(9, 2);
    lcd.print("PF:");
    lcd.print(formatFloat(data.power_factor, 2));
    
    // Line 3: Energy and Cost
    lcd.setCursor(0, 3);
    lcd.print("E:");
    lcd.print(formatFloat(data.daily_energy_kwh, 2));
    lcd.print("kWh");
    
    // Move to right half of line 3
    lcd.setCursor(9, 3);
    lcd.print("C:");
    lcd.print(formatFloat(data.daily_cost, 2));
    lcd.print("GHC");
    
    // Stale data indicator (top-right corner)
    if (millis() - data.timestamp > 10000) { // 10 seconds
        lcd.setCursor(15, 0);
        lcd.print("!");
    }
}

void LCDInterface::displayPage2(const PZEMReading& data) {
    // Clear the display first (optional)
    lcd.clear();
    
    // Line 0: Header (centered)
    lcd.setCursor(2, 0);
    lcd.print("TENANT B");
    
    // Line 1: Voltage and Current
    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(formatFloat(data.voltage, 1));
    lcd.print("V");
    
    lcd.setCursor(9, 1);
    lcd.print("I:");
    lcd.print(formatFloat(data.current, 2));
    lcd.print("A");
    
    // Line 2: Power and Power Factor
    lcd.setCursor(0, 2);
    lcd.print("P:");
    lcd.print(formatFloat(data.power, 1));
    lcd.print("W");
    
    lcd.setCursor(9, 2);
    lcd.print("PF:");
    lcd.print(formatFloat(data.power_factor, 2));
    
    // Line 3: Energy and Cost
    lcd.setCursor(0, 3);
    lcd.print("E:");
    lcd.print(formatFloat(data.daily_energy_kwh, 2));
    lcd.print("kWh");
    
    lcd.setCursor(9, 3);
    lcd.print("C:");
    lcd.print(formatFloat(data.daily_cost, 2));
    lcd.print("GHC");
    
    // Stale data indicator
    if (millis() - data.timestamp > 10000) {
        lcd.setCursor(15, 0);
        lcd.print("!");
    }
    
    // Threshold warning indicator
    if(data.daily_energy_kwh > DAILY_ENERGY_THRESHOLD * 0.8) {
        lcd.setCursor(14, 0);
        lcd.print("*");
    }
}

void LCDInterface::displayPage3(const PZEMResult& data) {
    // Clear the display first
    lcd.clear();
    
    // Line 0: Header (centered)
    lcd.setCursor(3, 0);
    lcd.print("SUMMARY");
    
    // Line 1: Total Power (centered)
    lcd.setCursor(0, 1);
    lcd.print(" Total Power: ");
    lcd.print(formatFloat(data.summary.total_power, 1));
    lcd.print("W ");
    
    // Line 2: Total Energy (centered)
    lcd.setCursor(0, 2);
    lcd.print("Energy: ");
    lcd.print(formatFloat(data.summary.total_daily_energy_kwh, 2));
    lcd.print("kWh");
    
    // Line 3: Total Cost (centered)
    lcd.setCursor(0, 3);
    lcd.print("Cost: ");
    lcd.print(formatFloat(data.summary.total_daily_cost, 2));
    lcd.print("GHC");
    
    // Warning indicators
    if (data.summary.total_daily_energy_kwh > DAILY_ENERGY_THRESHOLD * 0.8) {
        lcd.setCursor(15, 0);
        lcd.print("*");  // Warning indicator
        
        // Add visual alert on cost if also approaching threshold
        if (data.summary.total_daily_cost > DAILY_COST_THRESHOLD * 0.8) {
            lcd.setCursor(14, 0);
            lcd.print("!");
        }
    }
}

void LCDInterface::displayPage4(const StatusResult& status) {
    // Clear the display
    lcd.clear();
    
    // Line 0: Header (centered)
    lcd.setCursor(2, 0);
    lcd.print("SYSTEM STATUS");
    
    // Line 1: Sensor Status (enhanced)
    lcd.setCursor(0, 1);
    lcd.print("Units:");
    lcd.setCursor(8, 1);
    if (status.tenant_a_ok && status.tenant_b_ok) {
        lcd.print("A+B OK");
    } else if (status.tenant_a_ok) {
        lcd.print("A OK");
        lcd.setCursor(15, 1);
        lcd.print("!");  // Warning for B down
    } else if (status.tenant_b_ok) {
        lcd.print("B OK");
        lcd.setCursor(15, 1);
        lcd.print("!");  // Warning for A down
    } else {
        lcd.print("ERROR");
        lcd.setCursor(15, 1);
        lcd.print("!!"); // Critical error
    }
    
    // Line 2: Uptime (formatted as HH:MM:SS)
    unsigned long totalSeconds = millis() / 1000;
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    
    lcd.setCursor(0, 2);
    lcd.print("Uptime:");
    lcd.setCursor(8, 2);
    if (hours < 10) lcd.print("0");
    lcd.print(hours);
    lcd.print(":");
    if (minutes < 10) lcd.print("0");
    lcd.print(minutes);
    lcd.print(":");
    if (seconds < 10) lcd.print("0");
    lcd.print(seconds);
    
    // Line 3: Last Update + Memory
    lcd.setCursor(0, 3);
    lcd.print("Last:");
    lcd.print((millis() - lastUpdateTime) / 1000);
    lcd.print("s ");
    
    // Add free memory indicator if available
    #ifdef ESP32
    lcd.print((ESP.getFreeHeap() / 1024));
    lcd.print("KB");
    #endif
}

void LCDInterface::clearLine(int line) {
    lcd.setCursor(0, line);
    lcd.print("                "); // 16 spaces
}