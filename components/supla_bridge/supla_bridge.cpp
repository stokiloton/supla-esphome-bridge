#include "supla_bridge.h"
#include "esphome/core/log.h"

#include "SuplaDevice.h"
#include "supla/network/custom_tcp.h"
#include <WiFiClient.h>

namespace esphome {
namespace supla_bridge {

static const char *TAG = "supla_bridge";

void SuplaBridge::setup() {
  ESP_LOGI(TAG, "Initializing SUPLA Bridge");
  ESP_LOGI(TAG, "Server: %s", server_.c_str());
  ESP_LOGI(TAG, "User: %s", email_.c_str());

  // create WiFiClient and wrap it into Supla CustomTcp
  // keep them as members to ensure lifetime
  WiFiClient *wifi_client = new WiFiClient();
  custom_tcp_ = new Supla::Network::CustomTcp(wifi_client);
  network_client_ = custom_tcp_;

  // pass network client to SuplaDevice
  SuplaDevice.setNetworkClient(network_client_);

  // initialize SuplaDevice (GUID and authkey must be set elsewhere or in config)
  // here we call begin() without GUID/authkey to run default init
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
