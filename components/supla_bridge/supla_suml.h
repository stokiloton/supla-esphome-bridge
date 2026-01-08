#pragma once
#include <stdint.h>
#include <string.h>

static uint16_t supla_crc16(const uint8_t *data, int len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

#pragma pack(push, 1)

struct SuplaPacketHeader {
  uint8_t marker[3];   // 'S','U','P'
  uint8_t version;     // 1
  uint16_t data_size;
  uint16_t crc16;
};

#pragma pack(pop)

inline void supla_prepare_header(SuplaPacketHeader &hdr, uint16_t size, const uint8_t *payload) {
  hdr.marker[0] = 'S';
  hdr.marker[1] = 'U';
  hdr.marker[2] = 'P';
  hdr.version = 1;
  hdr.data_size = size;
  hdr.crc16 = supla_crc16(payload, size);
}
