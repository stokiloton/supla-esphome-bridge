#pragma once
#include <stdint.h>
#include <string.h>

// ------------------------------------------------------------
// SUM-L HEADER (używany w Twoim projekcie)
// ------------------------------------------------------------
//
// Format:
//   marker[3] = "SUP"
//   version   = 1
//   data_size = rozmiar payloadu
//   crc16     = CRC16(payload)
//
// Ten nagłówek jest Twoją własną warstwą transportową,
// nie pochodzi z oficjalnego SUPLA.
//

typedef struct {
    char     marker[3];   // 'S','U','P'
    uint8_t  version;     // zwykle 1
    uint16_t data_size;   // długość payloadu
    uint16_t crc16;       // CRC16(payload)
} SuplaPacketHeader;

// ------------------------------------------------------------
// GUID – używany w Twoim projekcie
// ------------------------------------------------------------

#define SUPLA_GUID_SIZE 16

typedef struct {
    uint8_t guid[SUPLA_GUID_SIZE];
} SuplaGuid;

// ------------------------------------------------------------
// CRC16 – zgodny z Twoją implementacją
// ------------------------------------------------------------

static inline uint16_t supla_crc16(const uint8_t *data, uint16_t len) {
    uint
