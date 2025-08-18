#include "SensorHandler.h"
#include "config.h"

SensorHandler::SensorHandler() : 
    pzemA(PZEM_A_RX_PIN, PZEM_A_TX_PIN),
    pzemB(PZEM_B_RX_PIN, PZEM_B_TX_PIN) {
    
    energyA = 0.0f;
    energyB = 0.0f;
    dailyEnergyA = 0.0f;
    dailyEnergyB = 0.0f;
    lastReadingA = 0;
    lastReadingB = 0;
    
    status.tenant_a_ok = false;
    status.tenant_b_ok = false;
    status.last_error = "";
    
    mockMode = false;
    mockCounter = 0;
}

SensorHandler::~SensorHandler() {
    if(pzemA.isListening()) pzemA.end();
    if(pzemB.isListening()) pzemB.end();
}

void SensorHandler::init() {
    if(!mockMode) {
        pzemA.begin(PZEM_UART_BAUDRATE);
        pzemB.begin(PZEM_UART_BAUDRATE);
    }
    
    // Initial status
    status.tenant_a_ok = !mockMode;
    status.tenant_b_ok = !mockMode;
    status.last_error = mockMode ? "Mock mode active" : "";
}

uint16_t SensorHandler::crc16Modbus(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for(uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void SensorHandler::buildReadCommand(uint8_t address, uint8_t *cmd) {
    cmd[0] = address;
    cmd[1] = 0x04; // Read input registers
    cmd[2] = 0x00; // Start address high
    cmd[3] = 0x00; // Start address low
    cmd[4] = 0x00; // Register count high
    cmd[5] = 0x0A; // Register count low (10 registers)
    
    uint16_t crc = crc16Modbus(cmd, 6);
    cmd[6] = crc & 0xFF;
    cmd[7] = (crc >> 8) & 0xFF;
}

void SensorHandler::buildWriteSingleCommand(uint8_t address, uint16_t reg, uint16_t value, uint8_t *cmd) {
    cmd[0] = address;
    cmd[1] = 0x06; // Write single register
    cmd[2] = (reg >> 8) & 0xFF; // Register address high
    cmd[3] = reg & 0xFF; // Register address low
    cmd[4] = (value >> 8) & 0xFF; // Value high
    cmd[5] = value & 0xFF; // Value low
    
    uint16_t crc = crc16Modbus(cmd, 6);
    cmd[6] = crc & 0xFF;
    cmd[7] = (crc >> 8) & 0xFF;
}

bool SensorHandler::sendAndReceive(SoftwareSerial &serial, uint8_t address, uint8_t *response, 
                                 uint8_t responseSize, uint16_t timeout) {
    if(mockMode) return false;
    
    uint8_t cmd[8];
    buildReadCommand(address, cmd);
    
    // Clear receive buffer
    while(serial.available()) serial.read();
    
    // Send command
    serial.write(cmd, sizeof(cmd));
    
    // Wait for response
    unsigned long start = millis();
    uint8_t idx = 0;
    
    while((millis() - start) < timeout) {
        if(serial.available()) {
            response[idx++] = serial.read();
            if(idx >= responseSize) {
                return true;
            }
        }
        delay(10);
    }
    
    return false;
}

bool SensorHandler::parseResponse(uint8_t *response, uint8_t len, uint8_t address, PZEMReading &result) {
    if(len < 25) return false;
    if(response[0] != address) return false;
    if(response[1] != 0x04) return false;
    if(response[2] != 20) return false;     // 20 bytes of data expected
    
    // Verify CRC
    uint16_t crc = crc16Modbus(response, 23);
    if((response[23] != (crc & 0xFF)) || (response[24] != ((crc >> 8) & 0xFF))) {
        return false;
    }
    
    // Add debug output to see raw bytes
    // if(DEBUG_MODE) {
    //     Serial.print("Raw response bytes: ");
    //     for(int i = 3; i <= 20; i++) {
    //         if(response[i] < 0x10) Serial.print("0");
    //         Serial.print(response[i], HEX);
    //         Serial.print(" ");
    //     }
    //     Serial.println();
    // }
    
    // Based on OFFICIAL PZEM-004T Manual:
    // All values use BIG-ENDIAN 16-bit words
    // 32-bit values are stored as [HIGH_WORD][LOW_WORD]
    
    // Parse voltage (register 0x0000) - 16-bit BIG-ENDIAN in 0.1V units
    uint16_t voltageRaw = ((uint16_t)response[3] << 8) | response[4];
    result.voltage = voltageRaw / 10.0f;

    // Parse current (registers 0x0001-0x0002) - 32-bit as [HIGH_WORD][LOW_WORD] in 0.001A units
    // LOW word (0x0001): bytes 5,6    HIGH word (0x0002): bytes 7,8
    uint16_t currentLow = ((uint16_t)response[5] << 8) | response[6];
    uint16_t currentHigh = ((uint16_t)response[7] << 8) | response[8];
    uint32_t currentRaw = ((uint32_t)currentHigh << 16) | currentLow;
    result.current = currentRaw / 1000.0f;
    
    // Parse power (registers 0x0003-0x0004) - 32-bit as [HIGH_WORD][LOW_WORD] in 0.1W units
    // LOW word (0x0003): bytes 9,10   HIGH word (0x0004): bytes 11,12
    uint16_t powerLow = ((uint16_t)response[9] << 8) | response[10];
    uint16_t powerHigh = ((uint16_t)response[11] << 8) | response[12];
    uint32_t powerRaw = ((uint32_t)powerHigh << 16) | powerLow;
    result.power = powerRaw / 10.0f;
    
    // Parse energy (registers 0x0005-0x0006) - 32-bit as [HIGH_WORD][LOW_WORD] in 1Wh units
    // LOW word (0x0005): bytes 13,14  HIGH word (0x0006): bytes 15,16
    uint16_t energyLow = ((uint16_t)response[13] << 8) | response[14];
    uint16_t energyHigh = ((uint16_t)response[15] << 8) | response[16];
    uint32_t energyRaw = ((uint32_t)energyHigh << 16) | energyLow;
    result.energy_wh_raw = energyRaw;
    result.energy_kwh = energyRaw / 1000.0f;
    
    // Parse frequency (register 0x0007) - 16-bit BIG-ENDIAN in 0.1Hz units
    uint16_t frequencyRaw = ((uint16_t)response[17] << 8) | response[18];
    result.frequency = frequencyRaw / 10.0f;
    
    // Parse power factor (register 0x0008) - 16-bit BIG-ENDIAN in 0.01 units
    uint16_t pfRaw = ((uint16_t)response[19] << 8) | response[20];
    result.power_factor = pfRaw / 100.0f;
    result.power_factor = constrain(result.power_factor, 0.0f, 1.0f);
    
    // Add detailed debug output
    // if(DEBUG_MODE) {
    //     Serial.println("Parsed values:");
    //     Serial.print("Voltage: 0x"); Serial.print(voltageRaw, HEX); 
    //     Serial.print(" = "); Serial.print(result.voltage); Serial.println("V");
        
    //     Serial.print("Current: LOW=0x"); Serial.print(currentLow, HEX); 
    //     Serial.print(" HIGH=0x"); Serial.print(currentHigh, HEX);
    //     Serial.print(" TOTAL=0x"); Serial.print(currentRaw, HEX);
    //     Serial.print(" = "); Serial.print(result.current, 3); Serial.println("A");
        
    //     Serial.print("Power: LOW=0x"); Serial.print(powerLow, HEX); 
    //     Serial.print(" HIGH=0x"); Serial.print(powerHigh, HEX);
    //     Serial.print(" TOTAL=0x"); Serial.print(powerRaw, HEX);
    //     Serial.print(" = "); Serial.print(result.power); Serial.println("W");
        
    //     Serial.print("Energy: LOW=0x"); Serial.print(energyLow, HEX); 
    //     Serial.print(" HIGH=0x"); Serial.print(energyHigh, HEX);
    //     Serial.print(" TOTAL=0x"); Serial.print(energyRaw, HEX);
    //     Serial.print(" = "); Serial.print(result.energy_kwh, 3); Serial.println("kWh");
        
    //     Serial.print("Frequency: 0x"); Serial.print(frequencyRaw, HEX); 
    //     Serial.print(" = "); Serial.print(result.frequency); Serial.println("Hz");
        
    //     Serial.print("PF: 0x"); Serial.print(pfRaw, HEX); 
    //     Serial.print(" = "); Serial.print(result.power_factor, 2); Serial.println("");
    //     Serial.println("---");
    // }
    
    result.timestamp = millis();
    result.ok = true;
    
    return true;
}

PZEMReading SensorHandler::readTenant(SoftwareSerial &serial, uint8_t address, 
                                     unsigned long &lastReading, float &energy, float &dailyEnergy) {
    PZEMReading result = emptyReading(energy);
    
    if(mockMode) {
        return mockRead();
    }
    
    uint8_t response[25];
    bool success = false;
    
    for(uint8_t attempt = 0; attempt < PZEM_RETRY_COUNT; attempt++) {
        if(sendAndReceive(serial, address, response, sizeof(response))) {
            if(parseResponse(response, sizeof(response), address, result)) {
                success = true;
                break;
            }
        }
        delay(50);
    }
    
if(!success) {
    status.last_error = "E" + String(address); // "E1" or "E2"
    return emptyReading(energy);
};
    
    // Energy accumulation
    unsigned long now = millis();
    if(lastReading > 0 && now > lastReading) {
        unsigned long elapsed = now - lastReading;
        if(elapsed <= 600000) { // Max 10 minutes between readings
            float deltaHours = elapsed / 3600000.0f; // ms to hours
            float deltaKwh = (result.power * deltaHours) / 1000.0f;
            energy += deltaKwh;
            dailyEnergy += deltaKwh;
        }
    }
    lastReading = now;
    
    // Update result with accumulated values
    result.energy_kwh = energy;
    result.daily_energy_kwh = dailyEnergy;
    result.daily_cost = dailyEnergy * ENERGY_COST_PER_KWH;
    
    return result;
}

PZEMReading SensorHandler::emptyReading(float energy) {
    PZEMReading result;
    result.voltage = 0.0f;
    result.current = 0.0f;
    result.power = 0.0f;
    result.energy_kwh = energy;
    result.energy_wh_raw = 0;
    result.daily_energy_kwh = 0.0f;
    result.daily_cost = 0.0f;
    result.frequency = 0.0f;
    result.power_factor = 0.0f;
    result.timestamp = millis();
    result.ok = false;
    return result;
}

PZEMReading SensorHandler::mockRead() {
    mockCounter++;
    
    float baseV = 230.0f;
    float variation = (mockCounter % 10) - 5;
    float voltage = baseV + variation * 0.2f;
    float current = 1.2f + ((mockCounter % 5) * 0.1f);
    float power = voltage * current;
    uint32_t energyWh = (mockCounter % 1000) * 10;
    
    PZEMReading result;
    result.voltage = round(voltage * 10) / 10.0f;
    result.current = round(current * 1000) / 1000.0f;
    result.power = round(power * 10) / 10.0f;
    result.energy_wh_raw = energyWh;
    result.energy_kwh = energyWh / 1000.0f;
    result.daily_energy_kwh = result.energy_kwh;
    result.daily_cost = result.daily_energy_kwh * ENERGY_COST_PER_KWH;
    result.frequency = 50.0f;
    result.power_factor = 0.95f;
    result.timestamp = millis();
    result.ok = true;
    
    return result;
}

PZEMResult SensorHandler::readAll() {
    PZEMResult result;
    
    result.tenant_a = readTenant(pzemA, PZEM_A_ADDRESS, lastReadingA, energyA, dailyEnergyA);
    result.tenant_b = readTenant(pzemB, PZEM_B_ADDRESS, lastReadingB, energyB, dailyEnergyB);

    // Update status
    status.tenant_a_ok = result.tenant_a.ok;
    status.tenant_b_ok = result.tenant_b.ok;
    
    // Create summary
    result.summary.total_power = result.tenant_a.power + result.tenant_b.power;
    result.summary.total_daily_energy_kwh = result.tenant_a.daily_energy_kwh + result.tenant_b.daily_energy_kwh;
    result.summary.total_daily_cost = result.tenant_a.daily_cost + result.tenant_b.daily_cost;
    result.summary.timestamp = max(result.tenant_a.timestamp, result.tenant_b.timestamp);
    
    return result;
}

void SensorHandler::resetDailyCounters() {
    dailyEnergyA = 0.0f;
    dailyEnergyB = 0.0f;
}

StatusResult SensorHandler::getStatus() {
    return status;
}

bool SensorHandler::setAddress(uint8_t oldAddr, uint8_t newAddr, uint8_t tenant) {
    // Implementation similar to MicroPython version
    // Would use buildWriteSingleCommand and send/receive logic
    return false; // Placeholder
}

uint8_t SensorHandler::discoverAddresses(uint8_t tenant) {
    SoftwareSerial* targetSerial = nullptr;
    uint8_t foundDevices = 0;

    // Determine which UART to scan
    if(tenant == 1 || (tenant == 0 && pzemA.isListening())) {
        targetSerial = &pzemA;
        Serial.println("Scanning PZEM A line...");
    } else if(tenant == 1 || (tenant == 0 && pzemB.isListening())) {
        targetSerial = &pzemB;
        Serial.println("Scanning PZEM B line...");
    } else {
        Serial.println("Error: No valid UART specified for discovery");
        return 0;
    }

    // Buffer for Modbus responses
    uint8_t response[25];
    
    Serial.println("Starting address discovery (1-247)...");
    Serial.println("Addr | Response | CRC Match");
    Serial.println("-----|----------|----------");

    for(uint8_t addr = 1; addr <= 247; addr++) {
        // Skip broadcast address 0
        if(addr == 0) continue;

        // Build and send read command
        uint8_t cmd[8];
        buildReadCommand(addr, cmd);
        
        // Clear buffer
        while(targetSerial->available()) targetSerial->read();
        
        // Send command
        targetSerial->write(cmd, sizeof(cmd));
        
        // Wait for response
        unsigned long start = millis();
        uint8_t idx = 0;
        while((millis() - start) < 500) { // 500ms timeout
            if(targetSerial->available()) {
                response[idx++] = targetSerial->read();
                if(idx >= sizeof(response)) break;
            }
            delay(10);
        }

        // Check if we got any response
        if(idx > 0) {
            // Verify CRC
            bool crcMatch = false;
            if(idx >= 4) { // Minimum response length
                uint16_t calcCrc = crc16Modbus(response, idx-2);
                crcMatch = (response[idx-2] == (calcCrc & 0xFF)) && 
                          (response[idx-1] == ((calcCrc >> 8) & 0xFF));
            }

            // Print discovery result
            Serial.print(addr);
            Serial.print("   | ");
            Serial.print(idx);
            Serial.print(" bytes  | ");
            Serial.println(crcMatch ? "YES" : "NO");

            if(crcMatch) {
                foundDevices++;
                Serial.print("--> Found device at address: ");
                Serial.println(addr);
            }
        } else {
            Serial.print(addr);
            Serial.println("   | No response");
        }
        
        delay(50); // Short delay between attempts
    }

    Serial.print("Discovery complete. Found ");
    Serial.print(foundDevices);
    Serial.println(" devices.");
    return foundDevices;
}

//  Diagnotics Function for both sensors
void SensorHandler::runDiagnostics() {
    Serial.println("=== PZEM DIAGNOSTIC MODE ===");
    
    // Test both sensors
    PZEMResult result = readAll();
    
    Serial.println("\n--- SENSOR A ---");
    Serial.print("Status: "); Serial.println(result.tenant_a.ok ? "OK" : "FAILED");
    Serial.print("Voltage: "); Serial.print(result.tenant_a.voltage); Serial.println("V");
    Serial.print("Current: "); Serial.print(result.tenant_a.current, 3); Serial.println("A");
    Serial.print("Power: "); Serial.print(result.tenant_a.power); Serial.println("W");
    Serial.print("PF: "); Serial.println(result.tenant_a.power_factor, 2);
    
    Serial.println("\n--- SENSOR B ---");
    Serial.print("Status: "); Serial.println(result.tenant_b.ok ? "OK" : "FAILED");
    Serial.print("Voltage: "); Serial.print(result.tenant_b.voltage); Serial.println("V");
    Serial.print("Current: "); Serial.print(result.tenant_b.current, 3); Serial.println("A");
    Serial.print("Power: "); Serial.print(result.tenant_b.power); Serial.println("W");
    Serial.print("PF: "); Serial.println(result.tenant_b.power_factor, 2);
    
    // Diagnostic conclusions
    Serial.println("\n--- DIAGNOSTICS ---");
    
    if(result.tenant_a.voltage > 200 && result.tenant_a.voltage < 250) {
        Serial.println("✓ Sensor A: Voltage normal - PZEM connected to mains");
    } else {
        Serial.println("✗ Sensor A: Voltage abnormal - Check mains connection");
    }
    
    if(result.tenant_a.current < 0.1) {
        Serial.println("⚠ Sensor A: Very low current - Check CT clamp installation");
        Serial.println("  - Ensure CT is clamped around ONLY the live wire");
        Serial.println("  - Check CT is properly closed");
        Serial.println("  - Try turning on a load (light, heater, etc.)");
    }
    
    if(result.tenant_b.voltage > 200 && result.tenant_b.voltage < 250) {
        Serial.println("✓ Sensor B: Voltage normal - PZEM connected to mains");
    } else {
        Serial.println("✗ Sensor B: Voltage abnormal - Check mains connection");
    }
    
    if(result.tenant_b.current < 0.1) {
        Serial.println("⚠ Sensor B: Very low current - Check CT clamp installation");
        Serial.println("  - Ensure CT is clamped around ONLY the live wire");
        Serial.println("  - Check CT is properly closed");
        Serial.println("  - Try turning on a load (light, heater, etc.)");
    }
    
    Serial.println("\n=== END DIAGNOSTICS ===\n");
}