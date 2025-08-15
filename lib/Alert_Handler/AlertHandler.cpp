#include "AlertHandler.h"

AlertHandler::AlertHandler() : 
    lastBlinkTime(0),
    alertActive(false),
    systemAlertActive(false),
    communicationActive(false) {}

void AlertHandler::begin() {
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    
    // Initial state
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

void AlertHandler::setSystemStatus(bool normalOperation) {
    if (normalOperation && !systemAlertActive) {
        digitalWrite(LED_GREEN_PIN, HIGH);
    } else {
        digitalWrite(LED_GREEN_PIN, LOW);
    }
}

void AlertHandler::setCommunicationStatus(bool active) {
    communicationActive = active;
    if (!alertActive && !systemAlertActive) {
        digitalWrite(LED_BLUE_PIN, active ? HIGH : LOW);
    }
}

void AlertHandler::triggerEnergyAlert(uint8_t tenant) {
    alertActive = true;
    if (DEBUG_MODE) {
        Serial.print("Energy alert triggered for tenant ");
        Serial.println(tenant);
    }
}

void AlertHandler::clearEnergyAlert() {
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
}

void AlertHandler::triggerSystemAlert() {
    systemAlertActive = true;
    if (DEBUG_MODE) {
        Serial.println("System alert triggered");
    }
}

void AlertHandler::clearSystemAlert() {
    systemAlertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
}

void AlertHandler::update() {
    unsigned long currentTime = millis();
    
    // Handle blinking for active alerts
    if (alertActive || systemAlertActive) {
        if (currentTime - lastBlinkTime >= LED_BLINK_INTERVAL) {
            lastBlinkTime = currentTime;
            
            // Toggle red LED for energy alerts
            if (alertActive) {
                digitalWrite(LED_RED_PIN, !digitalRead(LED_RED_PIN));
            }
            
            // Toggle buzzer for any active alert
            if (alertActive || systemAlertActive) {
                digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
            }
        }
    }
    
    // Blue LED behavior during communication
    if (communicationActive && !alertActive && !systemAlertActive) {
        if (currentTime - lastBlinkTime >= LED_BLINK_INTERVAL/2) {
            lastBlinkTime = currentTime;
            digitalWrite(LED_BLUE_PIN, !digitalRead(LED_BLUE_PIN));
        }
    }
}