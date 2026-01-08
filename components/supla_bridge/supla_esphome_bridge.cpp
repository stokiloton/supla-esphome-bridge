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

void SuplaEsphomeBridge::set_location_password(const std::string &hex) {
  memset(location_password_, 0, sizeof(location_password_));
  size_t len = std::min((size_t)32, hex.size());
  for (size_t i = 0; i + 1 < len && i / 2 < sizeof(location_password_); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    location_password_[i / 2] = (uint8_t) strtol(byte_str.c_str(), nullptr, 16);
  }
}

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
}

void SuplaEsphomeBridge::setup() {
  ESP_LOGI(TAG, "Setup SUPLA bridge");
  generate_guid_();
}

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
  // Czekamy na wynik rejestracji
  uint32_t start = millis();
  while (millis() - start < 5000) {
    if (client_.available() >= (int)(sizeof(SuplaPacketHeader) + sizeof(SuplaDeviceRegisterResult))) {
      SuplaPacketHeader hdr;
      client_.readBytes((uint8_t *)&hdr, sizeof(hdr));
      if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
        ESP_LOGW(TAG, "Invalid SUPLA header");
        break;
      }
      uint16_t size = hdr.data_size;
      if (size < sizeof(SuplaDeviceRegisterResult)) {
        ESP_LOGW(TAG, "Invalid register result size");
        break;
      }
      uint8_t buf[64];
      if (size > sizeof(buf)) {
        ESP_LOGW(TAG, "Register result too big");
        break;
      }
      client_.readBytes(buf, size);
      uint16_t crc = supla_crc16(buf, size);
      if (crc != hdr.crc16) {
        ESP_LOGW(TAG, "CRC mismatch in register result");
        break;
      }
      SuplaDeviceRegisterResult *res = (SuplaDeviceRegisterResult *)buf;
      if (res->type != SUPLA_SD_DEVICE_REGISTER_RESULT) {
        ESP_LOGW(TAG, "Unexpected packet type: %u", res->type);
        break;
      }
      if (res->result == 0) {
        ESP_LOGI(TAG, "SUPLA registration OK, device_id=%u", res->device_id);
        registered_ = true;
        return true;
      } else {
        ESP_LOGE(TAG, "SUPLA registration failed, result=%u", res->result);
        break;
      }
    }
    delay(10);
  }
  ESP_LOGE(TAG, "No register result from SUPLA");
  client_.stop();
  return false;
}

bool SuplaEsphomeBridge::send_register_() {
  SuplaDeviceRegister reg{};
  reg.type = SUPLA_SD_DEVICE_REGISTER;
  reg.proto_version = 1;
  reg.guid = guid_;
  reg.location_id = location_id_;
  memcpy(reg.location_password, location_password_, sizeof(location_password_));
  memset(reg.device_name, 0, sizeof(reg.device_name));
  strncpy(reg.device_name, device_name_.c_str(), sizeof(reg.device_name) - 1);
  reg.channel_count = 2;
  reg.channels[0].channel_number = 0;
  reg.channels[0].type = SUPLA_CHANNELTYPE_SENSOR_TEMP;
  reg.channels[1].channel_number = 1;
  reg.channels[1].type = SUPLA_CHANNELTYPE_RELAY;

  send_packet_((uint8_t *)&reg, sizeof(reg));
  ESP_LOGI(TAG, "SUPLA registration sent");
  return true;
}

void SuplaEsphomeBridge::send_packet_(const uint8_t *payload, uint16_t size) {
  SuplaPacketHeader hdr;
  supla_prepare_header(hdr, size, payload);
  client_.write((uint8_t *)&hdr, sizeof(hdr));
  client_.write(payload, size);
}

void SuplaEsphomeBridge::send_value_temp_() {
  if (!temp_sensor_ || !registered_ || !client_.connected())
    return;
  SuplaValueChangedTemp v{};
  v.type = SUPLA_SD_VALUE_CHANGED;
  v.channel_number = 0;
  v.value_type = 1;  // temp
  v.temperature = temp_sensor_->state;
  send_packet_((uint8_t *)&v, sizeof(v));
  ESP_LOGD(TAG, "Sent temperature: %.2f", v.temperature);
}

void SuplaEsphomeBridge::send_value_relay_() {
  if (!switch_light_ || !registered_ || !client_.connected())
    return;
  SuplaValueChangedRelay v{};
  v.type = SUPLA_SD_VALUE_CHANGED;
  v.channel_number = 1;
  v.value_type = 2;  // relay
  v.state = switch_light_->current_values.is_on() ? 1 : 0;
  send_packet_((uint8_t *)&v, sizeof(v));
  ESP_LOGD(TAG, "Sent relay state: %d", v.state);
}

void SuplaEsphomeBridge::send_ping_() {
  if (!client_.connected())
    return;
  SuplaPing p{};
  p.type = SUPLA_SD_PING_CLIENT;
  send_packet_((uint8_t *)&p, sizeof(p));
  ESP_LOGD(TAG, "Sent PING");
}

void SuplaEsphomeBridge::handle_incoming_() {
  while (client_.available() >= (int)sizeof(SuplaPacketHeader)) {
    SuplaPacketHeader hdr;
    client_.readBytes((uint8_t *)&hdr, sizeof(hdr));
    if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
      ESP_LOGW(TAG, "Invalid SUPLA header (loop)");
      return;
    }
    uint16_t size = hdr.data_size;
    if (size == 0 || size > 128) {
      ESP_LOGW(TAG, "Invalid packet size: %u", size);
      // odczytaj i wyrzuć
      uint8_t dump[128];
      int to_read = std::min((int)size, (int)sizeof(dump));
      client_.readBytes(dump, to_read);
      return;
    }
    uint8_t buf[128];
    client_.readBytes(buf, size);
    uint16_t crc = supla_crc16(buf, size);
    if (crc != hdr.crc16) {
      ESP_LOGW(TAG, "CRC mismatch in incoming packet");
      continue;
    }

    uint16_t type = *(uint16_t *)buf;
    if (type == SUPLA_SD_SET_VALUE) {
      if (size >= sizeof(SuplaSetValueRelay)) {
        SuplaSetValueRelay *cmd = (SuplaSetValueRelay *)buf;
        if (cmd->value_type == 2 && cmd->channel_number == 1 && switch_light_) {
          if (cmd->state) {
            switch_light_->turn_on().perform();
          } else {
            switch_light_->turn_off().perform();
          }
          ESP_LOGI(TAG, "Received relay command: %d", cmd->state);
        }
      }
    } else if (type == SUPLA_SD_PING_SERVER) {
      ESP_LOGD(TAG, "Received PING from server");
      // Można odpowiedzieć PING_CLIENT, ale mamy heartbeat
    } else {
      ESP_LOGD(TAG, "Received packet type: %u", type);
    }
  }
}

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
