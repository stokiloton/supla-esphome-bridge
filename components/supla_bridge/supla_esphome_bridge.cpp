#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

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
  
  // fallback – wypełnij czymś, żeby GUID nie był pusty
  for (int i = 0; i < 6; i++) mac[i] = i * 11;


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
  send_registration_();
  registered_ = true;
  ESP_LOGI(TAG, "SUPLA registration sent");
  return true;
}

void SuplaEsphomeBridge::send_registration_() {
  SuplaRegisterDevicePacket pkt{};
  pkt.version = 1;
  pkt.location_id = location_id_;
  memcpy(pkt.location_password, location_password_, sizeof(location_password_));
  pkt.guid = guid_;
  memset(pkt.device_name, 0, sizeof(pkt.device_name));
  strncpy(pkt.device_name, device_name_.c_str(), sizeof(pkt.device_name) - 1);

  pkt.channel_count = 2;
  pkt.channels[0].channel_number = 0;
  pkt.channels[0].type = SUPLA_CHANNELTYPE_SENSOR_TEMP;
  pkt.channels[1].channel_number = 1;
  pkt.channels[1].type = SUPLA_CHANNELTYPE_RELAY;

  client_.write((uint8_t *) &pkt, sizeof(pkt));
}

void SuplaEsphomeBridge::send_temperature_() {
  if (!temp_sensor_ || !registered_ || !client_.connected())
    return;
  SuplaTemperatureUpdate upd{};
  upd.type = 1;
  upd.channel_number = 0;
  upd.temperature = temp_sensor_->state;
  client_.write((uint8_t *) &upd, sizeof(upd));
  ESP_LOGD(TAG, "Sent temperature: %.2f", upd.temperature);
}

void SuplaEsphomeBridge::send_relay_state_() {
  if (!switch_light_ || !registered_ || !client_.connected())
    return;
  SuplaRelayUpdate upd{};
  upd.type = 2;
  upd.channel_number = 1;
  upd.state = switch_light_->current_values.is_on() ? 1 : 0;
  client_.write((uint8_t *) &upd, sizeof(upd));
  ESP_LOGD(TAG, "Sent relay state: %d", upd.state);
}

void SuplaEsphomeBridge::handle_incoming_() {
  while (client_.available() >= (int) sizeof(SuplaRelayCommand)) {
    SuplaRelayCommand cmd{};
    client_.readBytes((uint8_t *) &cmd, sizeof(cmd));
    if (cmd.type == 3 && cmd.channel_number == 1 && switch_light_) {
      if (cmd.state) {
        switch_light_->turn_on().perform();
      } else {
        switch_light_->turn_off().perform();
      }
      ESP_LOGI(TAG, "Received relay command: %d", cmd.state);
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
    send_temperature_();
    send_relay_state_();
  }
}

}  // namespace supla_esphome_bridge
}  // namespace esphome
