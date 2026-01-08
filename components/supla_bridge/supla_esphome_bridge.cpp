#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

namespace esphome {
namespace supla_esphome_bridge {

static const char *const TAG = "supla_esphome_bridge";

// ---------------------------------------------------------
// HEX DUMP
// ---------------------------------------------------------
static void hex_dump(const char *prefix, const uint8_t *data, int len) {
  ESP_LOGI(TAG, "%s (%d bytes):", prefix, len);
  char line[80];
  for (int i = 0; i < len; i += 16) {
    int pos = 0;
    pos += sprintf(line + pos, "%04X: ", i);
    for (int j = 0; j < 16 && i + j < len; j++)
      pos += sprintf(line + pos, "%02X ", data[i + j]);
    ESP_LOGI(TAG, "%s", line);
  }
}

// ---------------------------------------------------------
// PASSWORD PARSER
// ---------------------------------------------------------
void SuplaEsphomeBridge::set_location_password(const std::string &hex) {
  memset(location_password_, 0, sizeof(location_password_));
  size_t len = std::min((size_t)32, hex.size());
  for (size_t i = 0; i + 1 < len && i / 2 < sizeof(location_password_); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    location_password_[i / 2] = (uint8_t) strtol(byte_str.c_str(), nullptr, 16);
  }

  hex_dump("LOCATION PASSWORD", location_password_, 16);
}

// ---------------------------------------------------------
// GUID GENERATOR
// ---------------------------------------------------------
void SuplaEsphomeBridge::generate_guid_() {
  uint8_t mac[6];

#if defined(ESP8266) || defined(ESP32)
  WiFi.macAddress(mac);
#else
  for (int i = 0; i < 6; i++) mac[i] = i * 11;
#endif

  memset(guid_.guid, 0, sizeof(guid_.guid));
  memcpy(guid_.guid, mac, 6);

  uint32_t t = millis();
  memcpy(guid_.guid + 6, &t, sizeof(t));

  hex_dump("GUID", guid_.guid, 16);
}

// ---------------------------------------------------------
// SETUP
// ---------------------------------------------------------
void SuplaEsphomeBridge::setup() {
  ESP_LOGI(TAG, "Setup SUPLA bridge");
  generate_guid_();
}

// ---------------------------------------------------------
// CONNECT + REGISTER
// ---------------------------------------------------------
bool SuplaEsphomeBridge::connect_and_register_() {
  if (server_.empty()) {
    ESP_LOGE(TAG, "SUPLA server not set");
    return false;
  }

  ESP_LOGI(TAG, "Connecting to SUPLA server: %s", server_.c_str());
  if (!client_.connect(server_.c_str(), 2015)) {
    ESP_LOGW(TAG, "Connection to SUPLA failed");
    return false;
  }

  if (!send_register_()) {
    ESP_LOGE(TAG, "Failed to send register packet");
    client_.stop();
    return false;
  }

  ESP_LOGI(TAG, "Waiting for SUPLA register result...");

  uint32_t start = millis();
  while (millis() - start < 5000) {
    if (client_.available() >= (int)sizeof(SuplaPacketHeader)) {

      SuplaPacketHeader hdr;
      client_.readBytes((uint8_t *)&hdr, sizeof(hdr));

      ESP_LOGI(TAG, "RX HEADER: marker=%c%c%c version=%u size=%u crc=%04X",
               hdr.marker[0], hdr.marker[1], hdr.marker[2],
               hdr.version, hdr.data_size, hdr.crc16);

      if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
        ESP_LOGE(TAG, "Invalid SUPLA header");
        break;
      }

      uint16_t size = hdr.data_size;
      if (size == 0 || size > 256) {
        ESP_LOGE(TAG, "Invalid SUPLA payload size: %u", size);
        break;
      }

      uint8_t buf[256];
      client_.readBytes(buf, size);

      hex_dump("RX PAYLOAD", buf, size);

      uint16_t crc = supla_crc16(buf, size);
      ESP_LOGI(TAG, "CRC CALC=%04X  CRC HDR=%04X", crc, hdr.crc16);

      if (crc != hdr.crc16) {
        ESP_LOGE(TAG, "CRC mismatch!");
        break;
      }

      uint16_t type = *(uint16_t *)buf;
      ESP_LOGI(TAG, "Packet type: %u", type);

      if (type == SUPLA_SD_DEVICE_REGISTER_RESULT_B) {
        SuplaDeviceRegisterResult_B *res = (SuplaDeviceRegisterResult_B *)buf;

        ESP_LOGI(TAG, "REGISTER RESULT: result=%u device_id=%u channels=%u",
                 res->result_code, res->device_id, res->channel_count);

        if (res->result_code == 0) {
          registered_ = true;
          return true;
        } else {
          ESP_LOGE(TAG, "Registration failed: result=%u", res->result_code);
          break;
        }
      } else {
        ESP_LOGW(TAG, "Unexpected packet type during registration: %u", type);
      }
    }
    delay(10);
  }

  ESP_LOGE(TAG, "No register result from SUPLA");
  client_.stop();
  return false;
}

// ---------------------------------------------------------
// SEND REGISTER PACKET
// ---------------------------------------------------------
bool SuplaEsphomeBridge::send_register_() {
  SuplaDeviceRegister_B reg{};
  reg.type = SUPLA_SD_DEVICE_REGISTER_B;
  reg.proto_version = 17;

  memset(reg.software_version, 0, sizeof(reg.software_version));
  strncpy(reg.software_version, "1.0", sizeof(reg.software_version) - 1);

  reg.guid = guid_;
  reg.location_id = location_id_;
  memcpy(reg.location_password, location_password_, sizeof(location_password_));

  reg.manufacturer_id = 0;
  reg.product_id = 0;

  memset(reg.device_name, 0, sizeof(reg.device_name));
  strncpy(reg.device_name, device_name_.c_str(), sizeof(reg.device_name) - 1);

  reg.channel_count = 2;

  reg.channels[0].channel_number = 0;
  reg.channels[0].type = SUPLA_CHANNELTYPE_SENSOR_TEMP;
  reg.channels[0].value_type = SUPLA_VALUE_TYPE_DOUBLE;

  reg.channels[1].channel_number = 1;
  reg.channels[1].type = SUPLA_CHANNELTYPE_RELAY;
  reg.channels[1].value_type = SUPLA_VALUE_TYPE_ONOFF;

  hex_dump("TX REGISTER", (uint8_t *)&reg, sizeof(reg));

  send_packet_((uint8_t *)&reg, sizeof(reg));

  ESP_LOGI(TAG, "SUPLA registration sent");
  return true;
}

// ---------------------------------------------------------
// SEND PACKET (HEADER + PAYLOAD)
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_packet_(const uint8_t *payload, uint16_t size) {
  SuplaPacketHeader hdr;
  supla_prepare_header(hdr, size, payload);

  ESP_LOGI(TAG, "TX HEADER: size=%u crc=%04X", hdr.data_size, hdr.crc16);
  hex_dump("TX PAYLOAD", payload, size);

  client_.write((uint8_t *)&hdr, sizeof(hdr));
  client_.write(payload, size);
}

// ---------------------------------------------------------
// SEND TEMPERATURE
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_value_temp_() {
  if (!temp_sensor_ || !registered_ || !client_.connected())
    return;

  SuplaChannelValueChangedTemp_B v{};
  v.type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
  v.channel_number = 0;
  v.value_type = SUPLA_VALUE_TYPE_DOUBLE;
  v.temperature = (double) temp_sensor_->state;

  send_packet_((uint8_t *)&v, sizeof(v));
}

// ---------------------------------------------------------
// SEND RELAY STATE
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_value_relay_() {
  if (!switch_light_ || !registered_ || !client_.connected())
    return;

  SuplaChannelValueChangedRelay_B v{};
  v.type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
  v.channel_number = 1;
  v.value_type = SUPLA_VALUE_TYPE_ONOFF;
  v.state = switch_light_->current_values.is_on() ? 1 : 0;

  send_packet_((uint8_t *)&v, sizeof(v));
}

// ---------------------------------------------------------
// SEND PING
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_ping_() {
  if (!client_.connected())
    return;

  SuplaPing_B p{};
  p.type = SUPLA_SD_PING_CLIENT;

  send_packet_((uint8_t *)&p, sizeof(p));
}

// ---------------------------------------------------------
// HANDLE INCOMING PACKETS
// ---------------------------------------------------------
void SuplaEsphomeBridge::handle_incoming_() {
  while (client_.available() >= (int)sizeof(SuplaPacketHeader)) {

    SuplaPacketHeader hdr;
    client_.readBytes((uint8_t *)&hdr, sizeof(hdr));

    ESP_LOGI(TAG, "RX HEADER: size=%u crc=%04X", hdr.data_size, hdr.crc16);

    if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
      ESP_LOGE(TAG, "Invalid SUPLA header");
      return;
    }

    uint16_t size = hdr.data_size;
    if (size == 0 || size > 256) {
      ESP_LOGE(TAG, "Invalid packet size: %u", size);
      return;
    }

    uint8_t buf[256];
    client_.readBytes(buf, size);

    hex_dump("RX PAYLOAD", buf, size);

    uint16_t crc = supla_crc16(buf, size);
    ESP_LOGI(TAG, "CRC CALC=%04X  CRC HDR=%04X", crc, hdr.crc16);

    if (crc != hdr.crc16) {
      ESP_LOGE(TAG, "CRC mismatch");
      continue;
    }

    uint16_t type = *(uint16_t *)buf;
    ESP_LOGI(TAG, "RX packet type: %u", type);

    if (type == SUPLA_SD_CHANNEL_NEW_VALUE_B) {
      SuplaChannelNewValueRelay_B *cmd = (SuplaChannelNewValueRelay_B *)buf;

      ESP_LOGI(TAG, "SET VALUE: channel=%u state=%u",
               cmd->channel_number, cmd->state);

      if (cmd->channel_number == 1 && switch_light_) {
        if (cmd->state)
          switch_light_->turn_on().perform();
        else
          switch_light_->turn_off().perform();
      }
    }
  }
}

// ---------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------
void SuplaEsphomeBridge::loop() {
  if (!client_.connected()) {
    registered_ = false;
    uint32_t now = millis();
    if (now - last_reconnect_attempt_ > 10000) {
      last_reconnect_attempt_ = now;
      connect_and_register_();
    }
    return;
  }

  handle_incoming_();

  uint32_t now = millis();
  if (now - last_send_ > 10000) {
    last_send_ = now;
    send_value_temp_();
    send_value_relay_();
  }

  if (now - last_ping_ > 30000) {
    last_ping_ = now;
    send_ping_();
  }
}

}  // namespace supla_esphome_bridge
}  // namespace esphome
