#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <SuplaDevice.h>

namespace esphome {
namespace supla_bridge {

class SuplaBridge : public Component {
 public:
  std::string server_;
  std::string email_;
  std::string password_;
  bool switch_state_ = false;
  std::function<void(bool)> switch_callback_;

  void set_server(const std::string &server) { server_ = server; }
  void set_email(const std::string &email) { email_ = email; }
  void set_password(const std::string &password) { password_ = password; }

  void set_switch_callback(std::function<void(bool)> cb) {
    switch_callback_ = cb;
  }

  void setup() override {
    SuplaDevice.setName("ESPHome Bridge");

    SuplaDevice.addRelay([](int channel_number, bool on) {
      auto *bridge = (SuplaBridge *) SuplaDevice.getUserContext();
      if (bridge && bridge->switch_callback_) {
        bridge->switch_callback_(on);
      }
    });

    SuplaDevice.setUserContext(this);
    SuplaDevice.begin(email_.c_str(), password_.c_str(), server_.c_str());
  }

  void loop() override {
    SuplaDevice.iterate();
  }

  void update_switch(bool state) {
    switch_state_ = state;
    SuplaDevice.channelValueChanged(0, state);
  }
};

}  // namespace supla_bridge
}  // namespace esphome

