#include "supla_bridge.h"
#include "esphome/core/log.h"

#include "SuplaDevice.h"
#include "supla/network/custom_tcp.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#include "WiFiClient.h" // local stub for non-ESP builds
#endif

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

  // pass network client to SuplaDevice (SuplaDevice will use this client via ClientBuilder)
  SuplaDevice.setNetworkClient(s_custom_tcp);

  // initialize SuplaDevice (GUID/AuthKey must be provided via Supla storage or set before)
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
