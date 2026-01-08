#pragma once
#include <stdint.h>

#pragma pack(push, 1)

// Typy pakietów (uproszczone ID)
enum SuplaPacketType : uint16_t {
  SUPLA_SD_DEVICE_REGISTER       = 10,
  SUPLA_SD_DEVICE_REGISTER_RESULT= 20,
  SUPLA_SD_SET_VALUE             = 30,
  SUPLA_SD_VALUE_CHANGED         = 40,
  SUPLA_SD_PING_SERVER           = 50,
  SUPLA_SD_PING_CLIENT           = 51,
};

enum SuplaChannelType : uint8_t {
  SUPLA_CHANNELTYPE_SENSOR_TEMP = 10,
  SUPLA_CHANNELTYPE_RELAY       = 20
};

struct SuplaGuid {
  uint8_t guid[16];
};

struct SuplaChannelConfig {
  uint8_t channel_number;
  SuplaChannelType type;
};

struct SuplaDeviceRegister {
  uint16_t type;           // SUPLA_SD_DEVICE_REGISTER
  uint8_t proto_version;   // np. 1
  SuplaGuid guid;
  uint32_t location_id;
  uint8_t location_password[16];
  char device_name[32];
  uint8_t channel_count;
  SuplaChannelConfig channels[2];  // 0: temp, 1: relay
};

struct SuplaDeviceRegisterResult {
  uint16_t type;           // SUPLA_SD_DEVICE_REGISTER_RESULT
  uint8_t result;          // 0 = OK
  uint32_t device_id;
};

struct SuplaValueChangedTemp {
  uint16_t type;           // SUPLA_SD_VALUE_CHANGED
  uint8_t channel_number;
  uint8_t value_type;      // 1 = temp
  float temperature;
};

struct SuplaValueChangedRelay {
  uint16_t type;           // SUPLA_SD_VALUE_CHANGED
  uint8_t channel_number;
  uint8_t value_type;      // 2 = relay
  uint8_t state;           // 0/1
};

struct SuplaSetValueRelay {
  uint16_t type;           // SUPLA_SD_SET_VALUE
  uint8_t channel_number;
  uint8_t value_type;      // 2 = relay
  uint8_t state;           // 0/1
};

struct SuplaPing {
  uint16_t type;           // SUPLA_SD_PING_SERVER lub SUPLA_SD_PING_CLIENT
};

#pragma pack(pop)
