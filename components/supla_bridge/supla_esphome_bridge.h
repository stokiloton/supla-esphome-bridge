#pragma once

#include "esphome.h"
#include <WiFiClient.h>
#include <cstdint>
#include <cstring>
#include <string>

#include "proto.h"
#include "srpc.h"

namespace supla_esphome_bridge {

class SuplaEsphomeBridge : public esphome::Component {
 public:
  SuplaEsphomeBridge();
  ~SuplaEsphomeBridge();

  // Konfiguracja
  void set_server(const std::string &server) { server_ = server; }
  void set_location_id(int location_id) { location_id_ = location_id; }
  void set_location_password(const std::string &pwd) { location_password_ = pwd; }
  void set_device_name(const std::string &name) { device_name_ = name; }

  // Metody wymagane przez wygenerowany main.cpp
  void set_temperature_sensor(esphome::sensor::Sensor *s) { temperature_sensor_ = s; }
  void set_switch_light(esphome::light::LightState *l) { switch_light_ = l; }

  // Cykl życia komponentu
  void setup() override;
  void loop() override;

  // Ręczna rejestracja
  bool register_device(unsigned long timeout_ms = 10000);

  bool is_registered() const { return registered_; }

 private:
  bool send_register_packet(WiFiClient &client);
  bool read_register_response(WiFiClient &client, unsigned long timeout_ms);

  void hex_dump(const uint8_t *buf, size_t len, const char *prefix);

  std::string server_;
  int location_id_{0};
  std::string location_password_;
  std::string device_name_{"esphome-supla"};
  bool registered_{false};
  WiFiClient client_;

  esphome::sensor::Sensor *temperature_sensor_{nullptr};
  esphome::light::LightState *switch_light_{nullptr};

  // sproto/srpc context (typ zwracany przez sproto_init w Twoim srpc.h)
  void *sproto_ctx_{nullptr};

  // Stały GUID (binarnie)
  static const uint8_t GUID_BIN[SUPLA_GUID_SIZE];
};

}  // namespace supla_esphome_bridge
