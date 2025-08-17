
#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"

struct PZEMReading {
    float voltage;
    float current;
    float power;
    float energy_kwh;
    uint32_t energy_wh_raw;
    float daily_energy_kwh;
    float daily_cost;
    float frequency;
    float power_factor;
    unsigned long timestamp;
    bool ok;
};

struct PZEMResult {
    PZEMReading tenant_a;
    PZEMReading tenant_b;
    struct {
        float total_power;
        float total_daily_energy_kwh;
        float total_daily_cost;
        unsigned long timestamp;
    } summary;
};

struct StatusResult {
    bool tenant_a_ok;
    bool tenant_b_ok;
    String last_error;
};

class SensorHandler {
public:
    SensorHandler();
    ~SensorHandler();

    void init();
    PZEMResult readAll();
    void resetDailyCounters();
    void runDiagnostics();
    StatusResult getStatus();

    // Address management
    bool setAddress(uint8_t oldAddr, uint8_t newAddr, uint8_t tenant = 0); // 0=both, 1=A, 2=B
    uint8_t discoverAddresses(uint8_t tenant = 0); // Returns number of devices found

private:
    SoftwareSerial pzemA;
    SoftwareSerial pzemB;
    
    float energyA;
    float energyB;
    float dailyEnergyA;
    float dailyEnergyB;
    
    unsigned long lastReadingA;
    unsigned long lastReadingB;
    
    StatusResult status;

    // Mock mode
    bool mockMode;
    uint16_t mockCounter;
    
    // Private methods
    uint16_t crc16Modbus(const uint8_t *data, uint16_t len);
    void buildReadCommand(uint8_t address, uint8_t *cmd);
    void buildWriteSingleCommand(uint8_t address, uint16_t reg, uint16_t value, uint8_t *cmd);
    PZEMReading readTenant(SoftwareSerial &serial, uint8_t address, unsigned long &lastReading, 
                          float &energy, float &dailyEnergy);
    PZEMReading emptyReading(float energy = 0.0f);
    bool sendAndReceive(SoftwareSerial &serial, uint8_t address, uint8_t *response, 
                       uint8_t responseSize, uint16_t timeout = PZEM_RESPONSE_TIMEOUT);
    bool parseResponse(uint8_t *response, uint8_t len, uint8_t address, PZEMReading &result);
    PZEMReading mockRead();
};

#endif // SENSOR_HANDLER_H