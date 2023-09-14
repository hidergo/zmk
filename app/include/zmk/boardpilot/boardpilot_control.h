#pragma once

#include <stdint.h>

// Package header
// TODO: Do the messages need header on every chunk? large overhead but safer transfer
struct __attribute__((packed)) zmk_boardpilot_msg_header {
    // Report ID (should always be 0x05)
    uint8_t report_id; 
    // Command (enum zmk_control_cmd_t)
    uint8_t cmd;
    // Total message size
    uint16_t size;
    // Chunk size (actual message size)
    uint8_t chunk_size;
    // Message chunk offset
    uint16_t chunk_offset;
    // CRC8 - might not be necessary
    uint8_t crc;
};

// Maximum report size including 1 byte of report ID
#define ZMK_BOARDPILOT_REPORT_SIZE 0x20

// Size of the message without header
#define ZMK_BOARDPILOT_REPORT_DATA_SIZE (ZMK_BOARDPILOT_REPORT_SIZE - sizeof(struct zmk_boardpilot_msg_header))

// Set config message structure
struct __attribute__((packed)) zmk_boardpilot_msg_set_config {
    // Config key. config.h/enum zmk_config_key
    uint16_t key;
    // Config size
    uint16_t size;
    // Is the data to be saved to NVS
    uint8_t save;
    // Data. Represented as u8 but will be allocated to contain variable of length "size"
    uint8_t data;
};

// Get config message structure
struct __attribute__((packed)) zmk_boardpilot_msg_get_config {
    // Config key. config.h/enum zmk_config_key
    uint16_t key;
    // Config size
    uint16_t size;
    // Data. Represented as u8 but will be allocated to contain variable of length "size"
    uint8_t data;
};


/**
 * @brief Parses the message received from USB
 *
 * @param data Message data
 * @param len Message length
 *
 * @return 0 on success, error code on error
 */
int zmk_boardpilot_control_parse(const uint8_t *data, uint32_t len);
