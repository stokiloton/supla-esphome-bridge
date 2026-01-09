#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include "proto.h"   // ← plik w tym samym katalogu

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
  for (int i = 0; i < 6; i++)
    mac[i] = i * 11;
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
    if (client_.available() >= (int) sizeof(SuplaPacketHeader)) {
      SuplaPacketHeader hdr;
      client_.readBytes((uint8_t *) &hdr, sizeof(hdr));

      ESP_LOGI(TAG,
               "RX HEADER: marker=%c%c%c version=%u size=%u crc=%04X",
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
      ESP_LOGI(TAG, "CRC CALC=%04X CRC HDR=%04X", crc, hdr.crc16);

      if (crc != hdr.crc16) {
        ESP_LOGE(TAG, "CRC mismatch!");
        break;
      }

      uint16_t type = *(uint16_t *) buf;
      ESP_LOGI(TAG, "Packet type: %u", type);

      if (type == SUPLA_SD_DEVICE_REGISTER_RESULT_B) {
        auto *res = (TDS_SuplaDeviceRegisterResult_B *) buf;
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
// SEND REGISTER PACKET (proto 23: C + B + B + E)
// ---------------------------------------------------------

bool SuplaEsphomeBridge::send_register_() {
  // REGISTER_DEVICE_C – struktura z proto.h
  TDS_SuplaRegisterDevice_C reg{};
  memset(&reg, 0, sizeof(reg));

  reg.Version = 23;

  memcpy(reg.GUID, guid_.guid, SUPLA_GUID_SIZE);

  reg.LocationID = static_cast<int32_t>(location_id_);
  memcpy(reg.LocationPassword, location_password_, sizeof(reg.LocationPassword));

  reg.ManufacturerID = 0;
  reg.ProductID = 0;

  strncpy(reg.SoftVer, "1
