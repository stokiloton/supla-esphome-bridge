#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"

// forward declarations for Supla network types
namespace Supla {
class Client;
namespace Network {
class CustomTcp;
}  // namespace Network
}  // namespace Supla

// forward declare WiFiClient
class WiFiClient;

namespace esphome {
namespace supla_bridge {

class SuplaBridge : public Component {
 public:
  void set_server(const std::string &server) { server_ = server; }
  void set_email(const std::string &email) { email_ = email; }
  void set_password(const std::string &password) { password_ = password; }

  // Zwracamy triggery jako Trigger<>
  Trigger<> *get_turn_on_trigger() { return &on_turn_on_trigger_; }
  Trigger<> *get_turn_off_trigger() { return &on_turn_off_trigger_; }

  // Wywoływane z ESPHome (lambda w YAML) do wysłania stanu do SUPLA
  void update_switch(bool state);

  void setup() override;
  void loop() override;

 protected:
  std::string server_;
  std::string email_;
  std::string password_;

  // Triggery SUPLA -> ESPHome
  Trigger<> on_turn_on_trigger_;
  Trigger<> on_turn_off_trigger_;

  // tutaj możesz dodać obiekt SUPLA (np. SuplaDevice)

  // network client wrapper passed to SuplaDevice
  Supla::Client *network_client_ = nullptr;
  Supla::Network::CustomTcp *custom_tcp_ = nullptr;
  // keep WiFiClient to ensure its lifetime
  WiFiClient *wifi_client_ = nullptr;
};

}  // namespace supla_bridge
}  // namespace esphome
