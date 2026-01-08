#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <SuplaDevice.h>
#include <supla/control/virtual_relay.h>

namespace esphome {
namespace supla_bridge {

class SuplaBridge : public Component {
 public:
  std::string server_;
  std::string email_;
  std::string password_;

  Supla::Control::VirtualRelay *relay_{nullptr};

  Trigger<> *on_turn_on_trigger_{nullptr};
  Trigger<> *on_turn_off_trigger_{nullptr};

  void set_server(const std::string &server) { server_ = server; }
  void set_email(const std::string &email) { email_ = email; }
  void set_password(const std::string &password) { password_ = password; }

  void set_on_turn_on_trigger(Trigger<> *tr) { on_turn_on_trigger_ = tr; }
  void set_on_turn_off_trigger(Trigger<> *tr) { on_turn_off_trigger_ = tr; }

  void setup() override {
    // Nazwa urządzenia w SUPLI
    SuplaDevice.setName("ESPHome SUPLA Bridge");

    // Wirtualny przekaźnik (bez fizycznego pinu)
    relay_ = new Supla::Control::VirtualRelay();

    // Zmiana stanu z SUPLA -> ESPHome
    relay_->setOnChangeCallback([this](bool on) {
      if (on && this->on_turn_on_trigger_ != nullptr) {
        this->on_turn_on_trigger_->trigger();
      } else if (!on && this->on_turn_off_trigger_ != nullptr) {
        this->on_turn_off_trigger_->trigger();
      }
    });

    SuplaDevice.addChannel(relay_);
    SuplaDevice.begin(email_.c_str(), password_.c_str(), server_.c_str());
  }

  void loop() override {
    SuplaDevice.iterate();
  }

  // ESPHome -> SUPLA
  void update_switch(bool state) {
    if (relay_ != nullptr) {
      relay_->setState(state);
    }
  }
};

}  // namespace supla_bridge
}  // namespace esphome
