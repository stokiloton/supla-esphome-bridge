#pragma once

#include "esphome.h"
#include <WiFiClient.h>
#include <cstring>
#include "proto.h"
#include "srpc.h"

namespace supla_esphome_bridge {

class SuplaEsphomeBridge : public Component {
 public:
  SuplaEsphomeBridge();
  ~SuplaEsphomeBridge() override;

  void setup() override;
  void loop() override;

  // ESPHome setters (zgodne z __init__.py)
  void set_server(const std::string &server) { server_ = server; }
  void set_location_id(int location_id) { location_id_ = location_id; }
  void set_location_password(const std::string &pwd) { location_password_ = pwd; }
  void set_device_name(const std::string &name) { device_name_ = name; }

  // ręczna próba rejestracji (można wywołać z loop lub setup)
  bool register_device(unsigned long timeout_ms = 10000);

  bool is_registered() const { return registered_; }

 private:
  bool send_register_packet(WiFiClient &client);
  bool read_register_response(WiFiClient &client, unsigned long timeout_ms);

  // pomocnicze logowanie
  void hex_dump(const uint8_t *buf, size_t len, const char *prefix);

  std::string server_;
  int location_id_{0};
  std::string location_password_;
  std::string device_name_{"esphome-supla"};
  bool registered_{false};
  WiFiClient client_;
  void *sproto_ctx_{nullptr};

  // stały GUID (binarnie)
  static const uint8_t GUID_BIN[16];
};

}  // namespace supla_esphome_bridge
