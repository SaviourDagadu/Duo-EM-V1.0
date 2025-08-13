#include "PzemHandler.h"

// include vector for discoverAddresses result
#include <vector>

// Constructor - initialize members, but do not start UARTs (call begin() in setup)
PzemHandler::PzemHandler(uint8_t retry, uint16_t tMs, bool mockOverride)
    : SerialPZEMA(1), SerialPZEMB(2) // map to UART1 and UART2
{
    retryCount = retry;
    timeoutMs = tMs;
    // pick up config values where available
#ifdef PZEM_ADDRESS_A
    addrA = PZEM_ADDRESS_A;
#else
    addrA = 0x01;
#endif
#ifdef PZEM_ADDRESS_B
    addrB = PZEM_ADDRESS_B;
#else
    addrB = 0x02;
#endif

    energyA_kwh = energyB_kwh = 0.0;
    dailyEnergyA_kwh = dailyEnergyB_kwh = 0.0;
    lastReadingA_ts = lastReadingB_ts = 0;

    tenantAok = tenantBok = false;
    lastError = "";

    // decide mockMode: explicit override else from config if available
#ifdef MOCK_PZEM
    mockMode = MOCK_PZEM;
#else
    mockMode = false;
#endif
    if (mockOverride) mockMode = true;
}

// Destructor - ensure close called
PzemHandler::~PzemHandler() {
    close();
}

void PzemHandler::begin() {
    if (mockMode) {
        if (DEBUG_MODE) Serial.println("[PZEM] MOCK mode active, UARTs not initialized.");
        return;
    }
    // Initialize UARTs using pins from config.h
    // begin(baudrate, config, rxPin, txPin)
    SerialPZEMA.begin(PZEM_UART_BAUDRATE, PZEM_UART_CONFIG, PZEM_A_RX_PIN, PZEM_A_TX_PIN);
    SerialPZEMB.begin(PZEM_UART_BAUDRATE, PZEM_UART_CONFIG, PZEM_B_RX_PIN, PZEM_B_TX_PIN);
    if (DEBUG_MODE) {
        Serial.println("[PZEM] UARTs initialized for PZEM A (UART1) and PZEM B (UART2).");
    }
}

void PzemHandler::close() {
    if (!mockMode) {
        SerialPZEMA.end();
        SerialPZEMB.end();
    }
    if (DEBUG_MODE) Serial.println("[PZEM] Closed PZEM handler.");
}

void PzemHandler::resetDailyCounters() {
    dailyEnergyA_kwh = 0.0;
    dailyEnergyB_kwh = 0.0;
    if (DEBUG_MODE) Serial.println("[PZEM] Daily counters reset.");
}

void PzemHandler::getStatus(String &outStatus) {
    // quick JSON-like string for debugging; not strict JSON to avoid ArduinoJson dependency here
    outStatus = "{";
    outStatus += "\"tenant_a_ok\":" + String(tenantAok ? "true" : "false") + ",";
    outStatus += "\"tenant_b_ok\":" + String(tenantBok ? "true" : "false") + ",";
    outStatus += "\"last_error\":\"" + lastError + "\"}";
}

// Helper: gets time seconds - prefer RTC if available; fallback to millis()
unsigned long PzemHandler::nowSeconds() {
    // If you have an RTC/time library, replace this with actual epoch seconds.
    return millis() / 1000UL;
}

// CRC16-Modbus implementation (same polynomial as Python version)
uint16_t PzemHandler::crc16Modbus(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void PzemHandler::buildReadCommand(uint8_t address, uint8_t *outBuf, size_t &outLen) {
    // address, function 0x04, start 0x0000, 0x000A regs
    outBuf[0] = address;
    outBuf[1] = 0x04;
    outBuf[2] = 0x00;
    outBuf[3] = 0x00;
    outBuf[4] = 0x00;
    outBuf[5] = 0x0A;
    uint16_t crc = crc16Modbus(outBuf, 6);
    outBuf[6] = crc & 0xFF;
    outBuf[7] = (crc >> 8) & 0xFF;
    outLen = 8;
}

void PzemHandler::buildWriteSingle(uint8_t address, uint16_t reg, uint16_t value, uint8_t *outBuf, size_t &outLen) {
    outBuf[0] = address;
    outBuf[1] = 0x06;
    outBuf[2] = (reg >> 8) & 0xFF;
    outBuf[3] = reg & 0xFF;
    outBuf[4] = (value >> 8) & 0xFF;
    outBuf[5] = value & 0xFF;
    uint16_t crc = crc16Modbus(outBuf, 6);
    outBuf[6] = crc & 0xFF;
    outBuf[7] = (crc >> 8) & 0xFF;
    outLen = 8;
}

bool PzemHandler::sendAndReceive(HardwareSerial* uart, const uint8_t* cmd, size_t cmdLen,
                                 uint8_t* respBuf, size_t &respLen, uint16_t timeout) {
    if (!uart) return false;
    // flush
    while (uart->available()) uart->read();
    // write command
    uart->write(cmd, cmdLen);
    unsigned long start = millis();
    respLen = 0;
    // wait for bytes
    while (millis() - start < timeout) {
        if (uart->available()) {
            int r = uart->readBytes(respBuf + respLen, 1); // read one byte at a time
            if (r > 0) respLen += r;
            // expected 25 bytes full response
            if (respLen >= 25) break;
        }
    }
    return respLen > 0;
}

TenantReading PzemHandler::parseResponse(const uint8_t* response, size_t len, uint8_t address) {
    TenantReading out;
    // default empty
    out.voltage = out.current = out.power = 0.0;
    out.energy_kwh = out.daily_energy_kwh = out.daily_cost = 0.0;
    out.energy_wh_raw = 0;
    out.frequency = 0.0;
    out.power_factor = 0.0;
    out.timestamp = nowSeconds();
    out.ok = false;

    if (!response || len < 25) {
        return out;
    }
    if (response[0] != address) {
        if (DEBUG_MODE) Serial.printf("[PZEM] Response address mismatch %02X != %02X\n", response[0], address);
        return out;
    }
    if (response[1] != 0x04) {
        if (DEBUG_MODE) Serial.printf("[PZEM] Unexpected function code: %02X\n", response[1]);
        return out;
    }
    if (response[2] != 20) {
        if (DEBUG_MODE) Serial.printf("[PZEM] Unexpected byte count: %d\n", response[2]);
        return out;
    }
    // CRC check
    uint16_t calc = crc16Modbus(response, len - 2);
    uint16_t recv = response[len - 2] | (response[len - 1] << 8);
    if (calc != recv) {
        if (DEBUG_MODE) Serial.printf("[PZEM] CRC mismatch calc=%04X recv=%04X\n", calc, recv);
        return out;
    }
    // extract 20 bytes data (response[3..22])
    uint16_t regs[10];
    for (int i = 0; i < 10; ++i) {
        regs[i] = (uint16_t)((response[3 + i*2] << 8) | response[3 + i*2 + 1]);
    }
    // mapping as per Python: voltage = reg0/10
    float voltage = regs[0] / 10.0f;
    uint32_t current_raw = ((uint32_t)regs[2] << 16) | regs[1];
    float current = current_raw / 1000.0f;
    uint32_t power_raw = ((uint32_t)regs[4] << 16) | regs[3];
    float power = power_raw / 10.0f;
    uint32_t energy_raw = ((uint32_t)regs[6] << 16) | regs[5];
    uint32_t energy_wh = energy_raw;
    float frequency = regs[7] / 10.0f;
    float power_factor_raw = regs[8] / 100.0f;
    float power_factor = 0.0f;
    if (current == 0.0f || power == 0.0f) {
        power_factor = 0.0f;
    } else {
        power_factor = constrain(power_factor_raw, 0.0f, 1.0f);
    }
    out.voltage = voltage;
    out.current = current;
    out.power = power;
    out.energy_wh_raw = energy_wh;
    out.frequency = frequency;
    out.power_factor = power_factor;
    out.timestamp = nowSeconds();
    out.ok = true;
    return out;
}

TenantReading PzemHandler::mockRead() {
    static uint32_t mockCounter = 0;
    mockCounter++;
    TenantReading m;
    float base_v = 230.0f;
    int variation = (mockCounter % 10) - 5;
    float voltage = base_v + variation * 0.2f;
    float current = 1.2f + ((mockCounter % 5) * 0.1f);
    float power = voltage * current;
    uint32_t energy_wh = (mockCounter % 1000) * 10;
    m.voltage = roundf(voltage * 10.0f) / 10.0f;
    m.current = roundf(current * 1000.0f) / 1000.0f;
    m.power = roundf(power * 10.0f) / 10.0f;
    m.energy_wh_raw = energy_wh;
    m.frequency = 50.0f;
    m.power_factor = 0.95f;
    m.timestamp = nowSeconds();
    m.ok = true;
    return m;
}

// readTenant: will accumulate energy into energyA_kwh/energyB_kwh
TenantReading PzemHandler::readTenant(HardwareSerial* uart, uint8_t address,
                                      const String &lastReadingAttr,
                                      const String &energyAttr,
                                      const String &dailyEnergyAttr) {
    TenantReading empty;
    empty.ok = false;
    empty.timestamp = nowSeconds();

    if (mockMode) {
        TenantReading r = mockRead();
        // accumulate using lastReadingX timestamps
        unsigned long now = r.timestamp;
        unsigned long last = (address == addrA) ? lastReadingA_ts : lastReadingB_ts;
        if (last && now > last) {
            unsigned long dt = now - last;
            if (dt > 0 && dt <= 600) {
                double energy_kwh = (r.power * dt) / 3600000.0; // W * s / (3600*1000) -> kWh
                if (address == addrA) {
                    energyA_kwh += energy_kwh;
                    dailyEnergyA_kwh += energy_kwh;
                } else {
                    energyB_kwh += energy_kwh;
                    dailyEnergyB_kwh += energy_kwh;
                }
            }
        }
        if (address == addrA) lastReadingA_ts = now; else lastReadingB_ts = now;

        // fill accumulated fields
        if (address == addrA) {
            r.energy_kwh = energyA_kwh;
            r.daily_energy_kwh = dailyEnergyA_kwh;
            r.daily_cost = dailyEnergyA_kwh * ENERGY_RATE_GHS;
        } else {
            r.energy_kwh = energyB_kwh;
            r.daily_energy_kwh = dailyEnergyB_kwh;
            r.daily_cost = dailyEnergyB_kwh * ENERGY_RATE_GHS;
        }
        return r;
    }

    // not mock: attempt communications with retries
    uint8_t cmd[8];
    size_t cmdLen = 0;
    buildReadCommand(address, cmd, cmdLen);

    uint8_t respBuf[64];
    size_t respLen = 0;
    TenantReading parsed;
    bool got = false;

    for (uint8_t attempt = 0; attempt <= retryCount; ++attempt) {
        bool ok = sendAndReceive(uart, cmd, cmdLen, respBuf, respLen, timeoutMs);
        if (ok && respLen > 0) {
            parsed = parseResponse(respBuf, respLen, address);
            if (parsed.ok) {
                got = true;
                break;
            }
        }
        delay(50);
    }

    if (!got) {
        if (DEBUG_MODE) Serial.printf("[PZEM] No data for addr 0x%02X\n", address);
        lastError = "no_data_0x" + String(address, HEX);
        return empty;
    }

    // energy accumulation as in Python version
    unsigned long now = parsed.timestamp;
    unsigned long last = (address == addrA) ? lastReadingA_ts : lastReadingB_ts;
    if (last && now > last) {
        unsigned long dt = now - last;
        if (dt > 0 && dt <= 600) {
            // parsed.power in W
            double energy_kwh = (parsed.power * dt) / 3600000.0;
            if (address == addrA) {
                energyA_kwh += energy_kwh;
                dailyEnergyA_kwh += energy_kwh;
            } else {
                energyB_kwh += energy_kwh;
                dailyEnergyB_kwh += energy_kwh;
            }
        }
    }
    if (address == addrA) lastReadingA_ts = now; else lastReadingB_ts = now;

    // set accumulators into parsed
    if (address == addrA) {
        parsed.energy_kwh = energyA_kwh;
        parsed.daily_energy_kwh = dailyEnergyA_kwh;
        parsed.daily_cost = dailyEnergyA_kwh * ENERGY_RATE_GHS;
    } else {
        parsed.energy_kwh = energyB_kwh;
        parsed.daily_energy_kwh = dailyEnergyB_kwh;
        parsed.daily_cost = dailyEnergyB_kwh * ENERGY_RATE_GHS;
    }

    return parsed;
}

void PzemHandler::readAll(TenantReading &tenantA, TenantReading &tenantB, Summary &summary) {
    if (mockMode) {
        tenantA = readTenant(nullptr, addrA, "", "", "");
        tenantB = readTenant(nullptr, addrB, "", "", "");
    } else {
        tenantA = readTenant(&SerialPZEMA, addrA, "", "", "");
        tenantB = readTenant(&SerialPZEMB, addrB, "", "", "");
    }
    summary.total_power = roundf((tenantA.power + tenantB.power) * 100.0f) / 100.0f;
    summary.total_daily_energy_kwh = (double)tenantA.daily_energy_kwh + (double)tenantB.daily_energy_kwh;
    summary.total_daily_cost = (double)tenantA.daily_cost + (double)tenantB.daily_cost;
    summary.timestamp = max(tenantA.timestamp, tenantB.timestamp);
    tenantAok = tenantA.ok;
    tenantBok = tenantB.ok;
}

// discoverAddresses: returns true if at least one found; writes found addresses to vector
bool PzemHandler::discoverAddresses(HardwareSerial* uart, uint8_t start, uint8_t end,
                                    uint16_t timeoutMsLocal, uint8_t retries, std::vector<uint8_t> &found) {
    if (!uart) return false;
    uint8_t cmd[8];
    for (uint8_t addr = start; addr <= end; ++addr) {
        buildReadCommand(addr, cmd, *(size_t*)&cmd); // we don't care cmdLen here
        bool okFound = false;
        for (uint8_t attempt = 0; attempt <= retries; ++attempt) {
            // flush
            while (uart->available()) uart->read();
            size_t respLen = 0;
            uint8_t respBuf[64];
            uart->write(cmd, 8);
            unsigned long startMs = millis();
            while (millis() - startMs < timeoutMsLocal) {
                if (uart->available()) {
                    respLen += uart->readBytes(respBuf + respLen, 1);
                    if (respLen >= 25) break;
                }
            }
            if (respLen >= 1) {
                if (respBuf[0] == addr && (respBuf[1] == 0x04 || respBuf[1] == 0x84)) {
                    // optional CRC verify
                    if (respLen >= 25) {
                        uint16_t calc = crc16Modbus(respBuf, respLen - 2);
                        uint16_t recv = respBuf[respLen - 2] | (respBuf[respLen - 1] << 8);
                        if (calc == recv) {
                            found.push_back(addr);
                            okFound = true;
                            break;
                        }
                    } else {
                        // short response acceptable for discovery
                        found.push_back(addr);
                        okFound = true;
                        break;
                    }
                }
            }
            delay(20);
        }
        if (okFound && DEBUG_MODE) {
            Serial.printf("[PZEM] discover found addr 0x%02X\n", addr);
        }
    }
    return !found.empty();
}

bool PzemHandler::setAddress(HardwareSerial* uart, uint8_t oldAddr, uint8_t newAddr,
                             uint16_t regAddress, uint16_t timeoutMsLocal, bool verify) {
    if (!uart) return false;
    if (newAddr < 1 || newAddr > 247) {
        lastError = "new_addr_out_of_range";
        return false;
    }
    uint8_t cmd[8];
    size_t cmdLen = 0;
    buildWriteSingle(oldAddr, regAddress, newAddr, cmd, cmdLen);

    // flush and send
    while (uart->available()) uart->read();
    uart->write(cmd, cmdLen);
    delay(timeoutMsLocal);
    // check ack
    if (!uart->available()) {
        lastError = "no_ack_from_old_addr";
        return false;
    }
    uint8_t resp[32];
    size_t respLen = uart->readBytes(resp, sizeof(resp));
    if (respLen >= 8 && resp[0] == oldAddr && resp[1] == 0x06) {
        if (!verify) return true;
        // verify by reading with new address
        uint8_t readCmd[8];
        size_t readCmdLen = 0;
        buildReadCommand(newAddr, readCmd, readCmdLen);
        while (uart->available()) uart->read();
        uart->write(readCmd, readCmdLen);
        delay(timeoutMsLocal);
        if (uart->available()) {
            uint8_t vresp[64];
            size_t vlen = uart->readBytes(vresp, sizeof(vresp));
            if (vlen >= 25 && vresp[0] == newAddr) return true;
            lastError = "write_ack_but_verify_failed";
            return false;
        } else {
            lastError = "no_verify_response";
            return false;
        }
    } else {
        lastError = "no_write_ack";
        return false;
    }
}

void PzemHandler::setMockMode(bool mock) {
    mockMode = mock;
}
