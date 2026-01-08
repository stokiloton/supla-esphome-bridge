#include "supla_bridge.h"
#include "esphome/core/log.h"

#include "SuplaDevice.h"

namespace esphome {
namespace supla_bridge {

static const char *TAG = "supla_bridge";

void SuplaBridge::setup() {
  ESP_LOGI(TAG, "Initializing SUPLA Bridge");
  ESP_LOGI(TAG, "Server: %s", server_.c_str());
  ESP_LOGI(TAG, "User: %s", email_.c_str());

  // Initialize SuplaDevice (use library default network client)
  // GUID/AuthKey must be provided via Supla config/storage
  SuplaDevice.begin(23);
}

void SuplaBridge::loop() {
  // iterate SuplaDevice
  SuplaDevice.iterate();
}

void SuplaBridge::update_switch(bool state) {
  ESP_LOGI(TAG, "SUPLA: update_switch(%s)", state ? "ON" : "OFF");

  // tutaj wysyłasz stan do SUPLA
  // np. SuplaDevice.channelSetValue(...)

  // Przykład: SUPLA steruje ESPHome
  if (state) {
    ESP_LOGI(TAG, "Trigger: SUPLA -> ESPHome ON");
    on_turn_on_trigger_.trigger();
  } else {
    ESP_LOGI(TAG, "Trigger: SUPLA -> ESPHome OFF");
    on_turn_off_trigger_.trigger();
  }
}

}  // namespace supla_bridge
}  // namespace esphome
