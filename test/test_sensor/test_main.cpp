// #include <Arduino.h>
// #include "SensorHandler.h"

// SensorHandler sensorHandler;

// void printReading(const PZEMReading &reading);

// void setup() {
//     Serial.begin(115200);
//     while(!Serial); // Wait for serial port to connect
    
//     Serial.println("Starting SensorHandler test...");
    
//     // Initialize sensor handler
//     sensorHandler.init();
    
//     // Test mock mode
//     // sensorHandler.mockMode = true;
// }

// void loop() {
//     // Read sensors
//     PZEMResult result = sensorHandler.readAll();
    
//     // Print results
//     Serial.println("\n=== Tenant A ===");
//     printReading(result.tenant_a);
    
//     Serial.println("\n=== Tenant B ===");
//     printReading(result.tenant_b);
    
//     Serial.println("\n=== Summary ===");
//     Serial.print("Total Power: "); Serial.print(result.summary.total_power); Serial.println(" W");
//     Serial.print("Daily Energy: "); Serial.print(result.summary.total_daily_energy_kwh); Serial.println(" kWh");
//     Serial.print("Daily Cost: GHS "); Serial.println(result.summary.total_daily_cost);
    
//     // Print status
//     auto status = sensorHandler.getStatus();
//     Serial.println("\n=== Status ===");
//     Serial.print("Tenant A OK: "); Serial.println(status.tenant_a_ok ? "YES" : "NO");
//     Serial.print("Tenant B OK: "); Serial.println(status.tenant_b_ok ? "YES" : "NO");
//     Serial.print("Last Error: "); Serial.println(status.last_error);
    
//     delay(5000); // Read every 5 seconds
// }

// void printReading(const PZEMReading &reading) {
//     Serial.print("Voltage: "); Serial.print(reading.voltage); Serial.println(" V");
//     Serial.print("Current: "); Serial.print(reading.current); Serial.println(" A");
//     Serial.print("Power: "); Serial.print(reading.power); Serial.println(" W");
//     Serial.print("Energy: "); Serial.print(reading.energy_kwh); Serial.println(" kWh");
//     Serial.print("Daily Energy: "); Serial.print(reading.daily_energy_kwh); Serial.println(" kWh");
//     Serial.print("Daily Cost: GHS "); Serial.println(reading.daily_cost);
//     Serial.print("Frequency: "); Serial.print(reading.frequency); Serial.println(" Hz");
//     Serial.print("Power Factor: "); Serial.println(reading.power_factor);
//     Serial.print("Status: "); Serial.println(reading.ok ? "OK" : "ERROR");
// }