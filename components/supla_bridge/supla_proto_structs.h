#pragma once
#include <stdint.h>
#include <string.h>

// ------------------------------------------------------------
// Stałe zgodne z SUPLA proto 23 (minimalny zestaw)
// ------------------------------------------------------------

#define SUPLA_GUID_SIZE               16
#define SUPLA_LOCATION_PWD_MAXSIZE    32
#define SUPLA_SOFTVER_MAXSIZE         16
#define SUPLA_DEVICE_NAME_MAXSIZE     64

// Typy kanałów (wartości zgodne z oficjalnym protokołem)
#define SUPLA_CHANNELTYPE_SENSOR_TEMP 40
#define SUPLA_CHANNELTYPE_RELAY       10

// Typy wartości
#define SUPLA_VALUE_TYPE_DOUBLE       7
#define SUPLA_VALUE_TYPE_ONOFF        1

// ------------------------------------------------------------
// Podstawowe typy
// ------------------------------------------------------------

// Minimalna definicja GUID używana w Twoim projekcie
typedef struct {
    uint8_t value[SUPLA_GUID_SIZE];
} SuplaGuid;

// ------------------------------------------------------------
// Struktury rejestracyjne SUPLA proto 23
// ------------------------------------------------------------

// To jest payload dla „REGISTER_DEVICE_C” w protokole SUPLA.
// Uwaga: to nie jest pełny SRPC frame – tylko część danych.
typedef struct {
    uint8_t   Version;                                   // proto_version (23)
    SuplaGuid GUID;                                      // GUID urządzenia
    int32_t   LocationID;
    char      LocationPassword[SUPLA_LOCATION_PWD_MAXSIZE];
    int32_t   ManufacturerID;
    int32_t   ProductID;
    char      SoftVer[SUPLA_SOFTVER_MAXSIZE];
    char      Name[SUPLA_DEVICE_NAME_MAXSIZE];
    uint32_t  Flags;
    uint8_t   ChannelCount;
} TSD_SuplaRegisterDevice_C;


// Struktura pojedynczego kanału (odpowiednik ADD_CHANNEL_B)
typedef struct {
    uint8_t Number;
    uint8_t Type;
    uint8_t ValueType;
    uint8_t Flags;
} TSD_Channel_B;


// Zakończenie rejestracji (REGISTER_DEVICE_E – minimalne)
typedef struct {
    uint8_t reserved;
} TSD_SuplaRegisterDevice_E;

