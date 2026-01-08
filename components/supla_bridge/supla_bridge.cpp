#include "supla_bridge.h"
#include "esphome/core/log.h"

// Tell local SuplaDevice header that setNetworkClient is available
#define SUPLA_DEVICE_HAS_SETNETWORKCLIENT 1

#include "SuplaDevice.h"
#include "supla/network/custom_tcp.h"
#include <WiFiClient.h>

namespace esphome {
namespace supla_bridge {

static const char *TAG = "supla_bridge";

// Keep allocated clients alive for the runtime of the device
static WiFiClient *s_wifi_client = nullptr;
static Supla::Network::CustomTcp *s_custom_tcp = nullptr;

void SuplaBridge::setup() {
  ESP_LOGI(TAG, "Initializing SUPLA Bridge");
  ESP_LOGI(TAG, "Server: %s", server_.c_str());
  ESP_LOGI(TAG, "User: %s", email_.c_str());

  // create WiFiClient and wrap it into Supla CustomTcp
  if (s_wifi_client == nullptr) {
    s_wifi_client = new WiFiClient();
  }
  if (s_custom_tcp == nullptr) {
    s_custom_tcp = new Supla::Network::CustomTcp(s_wifi_client);
  }

  // Try to pass network client to SuplaDevice if available in Supla library
#if defined(SUPLA_DEVICE_HAS_SETNETWORKCLIENT)
  Supla::Client *network_client = s_custom_tcp;
  SuplaDevice.setNetworkClient(network_client);
#else
  ESP_LOGW(TAG,
           "SuplaDevice::setNetworkClient not available in this Supla lib; "
           "skipping custom client. Consider defining SUPLA_DEVICE_HAS_SETNETWORKCLIENT in SuplaDevice.h if supported.");
#endif

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
