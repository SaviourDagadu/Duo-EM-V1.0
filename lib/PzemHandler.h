#ifndef PZEM_HANDLER_H
#define PZEM_HANDLER_H

#include <Arduino.h>
#include <vector>

#include "config.h"

/*
  PZEMHandler - Dual-tenant PZEM-004T handler for ESP32 (Arduino framework)
  Mirrors the MicroPython implementation: readAll(), resetDailyCounters(),
  discoverAddresses(), setAddress(), getStatus(), close().

  Notes:
    - Uses raw Modbus RTU frames (build + CRC) same as the Python version.
    - Two HardwareSerial objects (UART1 & UART2) are used for PZEM A/B.
    - If config provides MOCK_PZEM = true, the handler returns deterministic
      fake readings for development/testing.
*/

struct TenantReading {
    float voltage;
    float current;
    float power;
    double energy_kwh;       // accumulator (kWh)
    uint32_t energy_wh_raw;  // raw Wh from device
    double daily_energy_kwh;
    double daily_cost;
    float frequency;
    float power_factor;
    unsigned long timestamp; // unix-like seconds (or millis()/1000 fallback)
    bool ok;
};

struct Summary {
    float total_power;
    double total_daily_energy_kwh;
    double total_daily_cost;
    unsigned long timestamp;
};

class PzemHandler {
public:
    // ctor: retry count, timeout in ms, optional mock override
    PzemHandler(uint8_t retry = 2, uint16_t timeoutMs = 500, bool mockOverride = false);
    ~PzemHandler();

    // Public API
    void begin(); // initialize UARTs (call in setup)
    void close(); // deinit UARTs when possible
    void resetDailyCounters();
    void getStatus(String &outStatus); // returns JSON-ish status in string
    bool discoverAddresses(HardwareSerial* uart, uint8_t start, uint8_t end,
                           uint16_t timeoutMs, uint8_t retries, std::vector<uint8_t> &found);
    bool setAddress(HardwareSerial* uart, uint8_t oldAddr, uint8_t newAddr,
                    uint16_t regAddress = 0x0002, uint16_t timeoutMs = 800, bool verify = true);

    // Read both tenants, return readings via references
    void readAll(TenantReading &tenantA, TenantReading &tenantB, Summary &summary);

    // Helper: if you want to read a single tenant
    TenantReading readTenant(HardwareSerial* uart, uint8_t address,
                             const String &lastReadingAttr = "",
                             const String &energyAttr = "",
                             const String &dailyEnergyAttr = "");

    // Optional: force mock on/off
    void setMockMode(bool mock);

private:
    // Low-level helpers
    uint16_t crc16Modbus(const uint8_t *data, size_t len);
    void buildReadCommand(uint8_t address, uint8_t *outBuf, size_t &outLen);
    void buildWriteSingle(uint8_t address, uint16_t reg, uint16_t value, uint8_t *outBuf, size_t &outLen);
    bool sendAndReceive(HardwareSerial* uart, const uint8_t* cmd, size_t cmdLen,
                        uint8_t* respBuf, size_t &respLen, uint16_t timeoutMs);

    TenantReading parseResponse(const uint8_t* response, size_t len, uint8_t address);
    TenantReading mockRead();

    // attributes copied from config.h
    uint8_t addrA, addrB;
    double energyA_kwh, energyB_kwh;
    double dailyEnergyA_kwh, dailyEnergyB_kwh;
    unsigned long lastReadingA_ts, lastReadingB_ts;

    bool tenantAok, tenantBok;
    String lastError;

    // UART handlers
    HardwareSerial SerialPZEMA; // uses UART1
    HardwareSerial SerialPZEMB; // uses UART2

    uint8_t retryCount;
    uint16_t timeoutMs;
    bool mockMode;

    // private helpers for timestamp
    unsigned long nowSeconds();
};

#endif // PZEM_HANDLER_H
