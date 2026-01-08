#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/light/light_state.h"
#include <WiFiClient.h>
#include "supla_protocol_minimal.h"

namespace esphome {
namespace supla_esphome_bridge {

class SuplaEsphomeBridge : public Component {
 public:
  void set_server(const std::string &server) { server_ = server; }
  void set_location_id(uint32_t id) { location_id_ = id; }
  void set_location_password(const std::string &hex);
  void set_device_name(const std::string &name) { device_name_ = name; }

  void set_temperature_sensor(sensor::Sensor *s) { temp_sensor_ = s; }
  void set_switch_light(light::LightState *l) { switch_light_ = l; }

  void setup() override;
  void loop() override;

 protected:
  std::string server_;
  uint32_t location_id_{0};
  uint8_t location_password_[16]{};
  std::string device_name_{"esphome"};

  SuplaGuid guid_{};
  WiFiClient client_;
  bool registered_{false};
  uint32_t last_send_{0};
  uint32_t last_reconnect_attempt_{0};

  sensor::Sensor *temp_sensor_{nullptr};
  light::LightState *switch_light_{nullptr};

  void generate_guid_();
  bool connect_and_register_();
  void send_registration_();
  void send_temperature_();
  void send_relay_state_();
  void handle_incoming_();
};

}  // namespace supla_esphome_bridge
}  // namespace esphome
