#pragma once
#include <stdint.h>

#pragma pack(push, 1)

// Typy pakietów (zgodne z ideą SUPLA, uproszczone ID – ale spójne w całym projekcie)
enum SuplaPacketType : uint16_t {
  SUPLA_SD_DEVICE_REGISTER_B          = 10,
  SUPLA_SD_DEVICE_REGISTER_RESULT_B   = 11,
  SUPLA_SD_CHANNEL_VALUE_CHANGED_B    = 20,
  SUPLA_SD_CHANNEL_NEW_VALUE_B        = 21,
  SUPLA_SD_PING_SERVER                = 30,
  SUPLA_SD_PING_CLIENT                = 31,
};

// Typy kanałów (uproszczone, ale stałe)
enum SuplaChannelType : uint8_t {
  SUPLA_CHANNELTYPE_SENSOR_TEMP       = 10,
  SUPLA_CHANNELTYPE_RELAY             = 20
};

// Typ wartości kanału
enum SuplaChannelValueType : uint8_t {
  SUPLA_VALUE_TYPE_NONE               = 0,
  SUPLA_VALUE_TYPE_DOUBLE             = 1,
  SUPLA_VALUE_TYPE_ONOFF              = 2,
};

// GUID urządzenia
struct SuplaGuid {
  uint8_t guid[16];
};

// Konfiguracja kanału (minimalna)
struct SuplaDeviceChannel_B {
  uint8_t channel_number;
  SuplaChannelType type;
  SuplaChannelValueType value_type;
};

// Pakiet rejestracji urządzenia (minimalny, wariant _B)
struct SuplaDeviceRegister_B {
  uint16_t type;             // SUPLA_SD_DEVICE_REGISTER_B
  uint8_t proto_version;     // 17
  char software_version[16]; // "1.0"
  SuplaGuid guid;
  uint32_t location_id;
  uint8_t location_password[16];
  uint16_t manufacturer_id;  // 0
  uint16_t product_id;       // 0
  char device_name[32];
  uint8_t channel_count;
  SuplaDeviceChannel_B channels[2];  // 0: temp, 1: relay
};

// Wynik rejestracji
struct SuplaDeviceRegisterResult_B {
  uint16_t type;             // SUPLA_SD_DEVICE_REGISTER_RESULT_B
  uint8_t result_code;       // 0 = OK
  uint32_t device_id;
  uint8_t channel_count;
};

// Wartość kanału – temperatura
struct SuplaChannelValueChangedTemp_B {
  uint16_t type;             // SUPLA_SD_CHANNEL_VALUE_CHANGED_B
  uint8_t channel_number;
  SuplaChannelValueType value_type;  // SUPLA_VALUE_TYPE_DOUBLE
  double temperature;
};

// Wartość kanału – przekaźnik
struct SuplaChannelValueChangedRelay_B {
  uint16_t type;             // SUPLA_SD_CHANNEL_VALUE_CHANGED_B
  uint8_t channel_number;
  SuplaChannelValueType value_type;  // SUPLA_VALUE_TYPE_ONOFF
  uint8_t state;             // 0/1
};

// Nowa wartość kanału – SET VALUE z chmury
struct SuplaChannelNewValueRelay_B {
  uint16_t type;             // SUPLA_SD_CHANNEL_NEW_VALUE_B
  uint8_t channel_number;
  SuplaChannelValueType value_type;  // SUPLA_VALUE_TYPE_ONOFF
  uint8_t state;             // 0/1
};

// PING
struct SuplaPing_B {
  uint16_t type;             // SUPLA_SD_PING_SERVER lub SUPLA_SD_PING_CLIENT
};

#pragma pack(pop)
