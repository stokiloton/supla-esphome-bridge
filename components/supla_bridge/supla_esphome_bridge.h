#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/light/light_state.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecureBearSSL.h>
  using SecureClient = BearSSL::WiFiClientSecure;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  using SecureClient = WiFiClientSecure;
#else
  #include <WiFiClientSecure.h>
  using SecureClient = WiFiClientSecure;
#endif

#include <string>
#include <cstdint>
#include <cstring>

#include "proto.h"

namespace esphome {
namespace supla_esphome_bridge {

class SuplaEsphomeBridge : public Component {
 public:
  SuplaEsphomeBridge() = default;
  ~SuplaEsphomeBridge() = default;

  // Konfiguracja
  void set_server(const std::string &server) { server_ = server; }
  void set_location_id(uint32_t id) { location_id_ = id; }
  void set_location_password(const std::string &hex);
  void set_device_name(const std::string &name) { device_name_ = name; }

  void set_temperature_sensor(sensor::Sensor *s) { temp_sensor_ = s; }
  void set_switch_light(light::LightState *l) { switch_light_ = l; }

  // ESPHome lifecycle
  void setup() override;
  void loop() override;

 protected:
  // Konfiguracja i zasoby
  std::string server_;
  uint32_t location_id_{0};
  uint8_t location_password_[16]{};
  std::string device_name_{"esphome"};

  SecureClient client_;
  
};
}  // namespace supla_esphome_bridge
}  // namespace esphome
