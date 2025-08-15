// #include <Arduino.h>
// #include "GSMModule.h"

// GSMModule gsm;

// void setup() {
//     Serial.begin(115200);
//     while (!Serial);
    
//     Serial.println("Starting GSM module test...");
    
//     if (gsm.initialize()) {
//         Serial.println("GSM module initialized successfully");
        
//         // Test SMS
//         if (gsm.sendSMS("+233XXXXXXXXX", "Test message from ESP32")) {
//             Serial.println("SMS sent successfully");
//         } else {
//             Serial.println("Failed to send SMS");
//         }
        
//         // Test threshold alert
//         gsm.sendThresholdAlert("A", "energy", 35.5f, 30.0f);
        
//         // Test status
//         auto status = gsm.getStatus();
//         Serial.println("\nGSM Module Status:");
//         Serial.print("Signal Strength: ");
//         Serial.println(status.signalStrength);
//         Serial.print("Operator: ");
//         Serial.println(status.operatorName);
//     } else {
//         Serial.println("Failed to initialize GSM module");
//     }
// }

// void loop() {
//     // Nothing here for test
// }