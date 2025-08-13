/**
 * @file gsm_handler.h
 * @brief GSM (SIM800L) module handler for ESP32
 * @details This module manages initialization, AT command communication,
 *          and data handling for the SIM800L GSM module using ESP32 hardware serial (UART2).
 * @version 1.0
 * @date 2025-08-13
 * 
 * Pin mappings are defined in config.h to allow easy hardware changes without editing this file.
 * 
 * Usage:
 *  1. Call gsm_init() in setup().
 *  2. Use gsm_send_sms(), gsm_send_at_command(), etc., from main.cpp or other modules.
 */

#ifndef GSM_HANDLER_H
#define GSM_HANDLER_H

#include <Arduino.h>
#include "config.h"  // Pull in GSM_TX_PIN, GSM_RX_PIN, GSM_BAUDRATE

// =========================
// Public Functions
// =========================

/**
 * @brief Initializes the GSM module over hardware UART.
 * @details Sets up UART2 with the defined TX/RX pins and baud rate.
 */
void gsm_init();

/**
 * @brief Sends a raw AT command to the GSM module.
 * @param command The AT command string to send (without "\r\n").
 */
void gsm_send_at_command(const String &command);

/**
 * @brief Reads a response from the GSM module (blocking).
 * @param timeout_ms How long to wait for a response (in milliseconds).
 * @return String containing the response.
 */
String gsm_read_response(uint32_t timeout_ms = 2000);

/**
 * @brief Sends an SMS message.
 * @param phone_number Recipient phone number in international format (+233...).
 * @param message The message body.
 */
void gsm_send_sms(const String &phone_number, const String &message);

#endif // GSM_HANDLER_H
