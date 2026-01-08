#pragma once
#include <stdint.h>
#include <string.h>
#include "supla_crc16.h"

#pragma pack(push, 1)

struct SuplaPacketHeader {
  uint8_t marker[3];   // 'S','U','P'
  uint8_t version;     // 17 dla proto v17
  uint16_t data_size;
  uint16_t crc16;
};

#pragma pack(pop)

static inline void supla_prepare_header(SuplaPacketHeader &hdr, uint16_t size, const uint8_t *payload) {
  hdr.marker[0] = 'S';
  hdr.marker[1] = 'U';
  hdr.marker[2] = 'P';
  hdr.version = 17;  // proto_version = 17
  hdr.data_size = size;
  hdr.crc16 = supla_crc16(payload, size);
}
