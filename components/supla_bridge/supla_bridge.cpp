#include "supla_bridge.h"
#include "esphome/core/log.h"

namespace esphome {
namespace supla_bridge {

static const char *TAG = "supla_bridge";

void SuplaBridge::setup() {
  ESP_LOGI(TAG, "Initializing SUPLA Bridge");
  ESP_LOGI(TAG, "Server: %s", server_.c_str());
  ESP_LOGI(TAG, "User: %s", email_.c_str());

  // tutaj inicjalizacja SUPLA (np. SuplaDevice.begin)
}

void SuplaBridge::loop() {
  // tutaj obsługa SUPLA (np. SuplaDevice.iterate)
}

void SuplaBridge::update_switch(bool state) {
  ESP_LOGI(TAG, "SUPLA: update_switch(%s)", state ? "ON" : "OFF");

  // tutaj wysyłasz stan do SUPLA
  // np. SuplaDevice.channelSetValue(...)

  // a jeśli SUPLA ma sterować ESPHome:
  if (state) {
    if (on_turn_on_trigger_) {
      ESP_LOGI(TAG, "Trigger: SUPLA -> ESPHome ON");
      on_turn_on_trigger_->fire();
    }
  } else {
    if (on_turn_off_trigger_) {
      ESP_LOGI(TAG, "Trigger: SUPLA -> ESPHome OFF");
      on_turn_off_trigger_->fire();
    }
  }
}

}  // namespace supla_bridge
}  // namespace esphome
