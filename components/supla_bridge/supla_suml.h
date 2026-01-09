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
// Ten nagłówek NIE pochodzi z oficjalnego SUPLA,
// to Twój własny framing używany w supla-esphome-bridge.
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
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}

// ------------------------------------------------------------
// Przygotowanie nagłówka SUM-L
// ------------------------------------------------------------

static inline void supla_prepare_header(SuplaPacketHeader &hdr,
                                        uint16_t size,
                                        const uint8_t *payload) {
    hdr.marker[0] = 'S';
    hdr.marker[1] = 'U';
    hdr.marker[2] = 'P';
    hdr.version   = 1;
    hdr.data_size = size;
    hdr.crc16     = supla_crc16(payload, size);
}
