#pragma once
#include <stdint.h>
#include <string.h>

// ------------------------------------------------------------
// Stałe zgodne z SUPLA proto 23
// ------------------------------------------------------------

#define SUPLA_GUID_SIZE               16
#define SUPLA_LOCATION_PWD_MAXSIZE    32
#define SUPLA_SOFTVER_MAXSIZE         16
#define SUPLA_DEVICE_NAME_MAXSIZE     64

// Typy kanałów
#define SUPLA_CHANNELTYPE_SENSOR_TEMP  40
#define SUPLA_CHANNELTYPE_RELAY        10

// Typy wartości
#define SUPLA_VALUE_TYPE_DOUBLE        7
#define SUPLA_VALUE_TYPE_ONOFF         1

// ------------------------------------------------------------
// Struktury rejestracyjne SUPLA proto 23
// ------------------------------------------------------------

// REGISTER_DEVICE_C
typedef struct {
    uint8_t  Version;
    uint8_t  GUID[SUPLA_GUID_SIZE];
    int32_t  LocationID;
    char     LocationPassword[SUPLA_LOCATION_PWD_MAXSIZE];
    int32_t  ManufacturerID;
    int32_t  ProductID;
    char     SoftVer[SUPLA_SOFTVER_MAXSIZE];
    char     Name[SUPLA_DEVICE_NAME_MAXSIZE];
    uint32_t Flags;
    uint8_t  ChannelCount;
} TSD_SuplaRegisterDevice_C;


// CHANNEL_B (dodawanie kanałów)
typedef struct {
    uint8_t Number;
    uint8_t Type;
    uint8_t ValueType;
    uint8_t Flags;
} TSD_Channel_B;


// REGISTER_DEVICE_E (koniec rejestracji)
typedef struct {
    uint8_t reserved;
} TSD_SuplaRegisterDevice_E;

