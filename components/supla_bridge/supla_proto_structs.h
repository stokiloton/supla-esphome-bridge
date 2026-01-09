#pragma once
#include <stdint.h>

// ------------------------------------------------------------
// Stałe kanałów – zgodne z SUPLA
// ------------------------------------------------------------

#define SUPLA_CHANNELTYPE_SENSOR_TEMP  40
#define SUPLA_CHANNELTYPE_RELAY        10

// ------------------------------------------------------------
// Typy wartości – zgodne z SUPLA
// ------------------------------------------------------------

#define SUPLA_VALUE_TYPE_DOUBLE        7
#define SUPLA_VALUE_TYPE_ONOFF         1

// ------------------------------------------------------------
// Typy ramek SUPLA (Twoje własne, zgodne z repo)
// ------------------------------------------------------------

#define SUPLA_SD_DEVICE_REGISTER_RESULT_B   70
#define SUPLA_SD_CHANNEL_VALUE_CHANGED_B    80
#define SUPLA_SD_CHANNEL_NEW_VALUE_B        90
#define SUPLA_SD_PING_CLIENT                100

// ------------------------------------------------------------
// Struktury VALUE_CHANGED – Twoje własne
// ------------------------------------------------------------

typedef struct {
    uint16_t type;           // SUPLA_SD_CHANNEL_VALUE_CHANGED_B
    uint8_t  channel_number;
    uint8_t  value_type;     // SUPLA_VALUE_TYPE_DOUBLE
    double   temperature;
} SuplaChannelValueChangedTemp_B;

typedef struct {
    uint16_t type;           // SUPLA_SD_CHANNEL_VALUE_CHANGED_B
    uint8_t  channel_number;
    uint8_t  value_type;     // SUPLA_VALUE_TYPE_ONOFF
    uint8_t  state;
} SuplaChannelValueChangedRelay_B;

// ------------------------------------------------------------
// Struktura NEW_VALUE – Twoja własna
// ------------------------------------------------------------

typedef struct {
    uint16_t type;           // SUPLA_SD_CHANNEL_NEW_VALUE_B
    uint8_t  channel_number;
    uint8_t  value_type;
    uint8_t  state;
} SuplaChannelNewValueRelay_B;

// ------------------------------------------------------------
// Struktura REGISTER_RESULT – Twoja własna
// ------------------------------------------------------------

typedef struct {
    uint16_t type;           // SUPLA_SD_DEVICE_REGISTER_RESULT_B
    uint8_t  result_code;
    uint32_t device_id;
    uint8_t  channel_count;
} SuplaDeviceRegisterResult_B;

// ------------------------------------------------------------
// Struktura PING – Twoja własna
// ------------------------------------------------------------

typedef struct {
    uint16_t type;           // SUPLA_SD_PING_CLIENT
} SuplaPing_B;
