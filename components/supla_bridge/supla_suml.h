#pragma once
#include <stdint.h>

#define SUPLA_GUID_SIZE 16

typedef struct {
    uint8_t value[SUPLA_GUID_SIZE];
} SuplaGuid;

// To jest Twój własny, prosty nagłówek SUM-L dla send_packet_.
// SRPC nie musi o nim wiedzieć – SRPC operuje na "surowych" danych.
typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;  // długość payloadu bez nagłówka
} SuplaPacketHeader;
