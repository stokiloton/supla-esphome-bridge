#pragma once
#include <Arduino.h>

#pragma pack(push, 1)

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

struct SuplaRegisterDevicePacket {
  uint8_t version;
  uint32_t location_id;
  uint8_t location_password[16];
  SuplaGuid guid;
  char device_name[32];
  uint8_t channel_count;
  SuplaChannelConfig channels[2];  // 0: temp, 1: relay
};

struct SuplaTemperatureUpdate {
  uint8_t type;          // 1 = temp update
  uint8_t channel_number;
  float temperature;
};

struct SuplaRelayUpdate {
  uint8_t type;          // 2 = relay update
  uint8_t channel_number;
  uint8_t state;         // 0/1
};

struct SuplaRelayCommand {
  uint8_t type;          // 3 = relay command
  uint8_t channel_number;
  uint8_t state;         // 0/1
};

#pragma pack(pop)
