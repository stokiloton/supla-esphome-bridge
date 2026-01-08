#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace supla_bridge {

class SuplaTurnOnTrigger : public Trigger<> {
 public:
  void fire() { this->trigger(); }
};

class SuplaTurnOffTrigger : public Trigger<> {
 public:
  void fire() { this->trigger(); }
};

class SuplaBridge : public Component {
 public:
  void set_server(const std::string &server) { server_ = server; }
  void set_email(const std::string &email) { email_ = email; }
  void set_password(const std::string &password) { password_ = password; }

  void set_on_turn_on_trigger(SuplaTurnOnTrigger *trig) { on_turn_on_trigger_ = trig; }
  void set_on_turn_off_trigger(SuplaTurnOffTrigger *trig) { on_turn_off_trigger_ = trig; }

  void update_switch(bool state);

  void setup() override;
  void loop() override;

 protected:
  std::string server_;
  std::string email_;
  std::string password_;

  SuplaTurnOnTrigger *on_turn_on_trigger_{nullptr};
  SuplaTurnOffTrigger *on_turn_off_trigger_{nullptr};

  // tutaj możesz dodać obiekt SUPLA (np. SuplaDevice)
};

}  // namespace supla_bridge
}  // namespace esphome
