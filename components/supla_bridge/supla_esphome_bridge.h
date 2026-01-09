#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/light/light_state.h"

#include <WiFiClient.h>
#include <string>
#include <cstdint>
#include <cstring>

#include "supla_suml.h"
#include "proto.h"

namespace esphome {
namespace supla_esphome_bridge {

class SuplaEsphomeBridge : public Component {
 public:
  SuplaEsphomeBridge() = default;
  ~SuplaEsphomeBridge() override = default;

  // Konfiguracja
  void set_server(const std::string &server) { server_ = server; }
  void set_location_id(uint32_t id) { location_id_ = id; }
  void set_location_password(const std::string &hex);
  void set_device_name(const std::string &name) { device_name_ = name; }

  void set_temperature_sensor(sensor::Sensor *s) { temp_sensor_ = s; }
  void set_switch_light(light::LightState *l) { switch_light_ = l; }

  // ESPHome lifecycle
  void setup() override;
  void loop() override;

 protected:
  // --- public / protected konfiguracja i zasoby ---
  std::string server_;
  uint32_t location_id_{0};
  uint8_t location_password_[16]{};
  std::string device_name_{"esphome"};

  SuplaGuid guid_{};

  WiFiClient client_;
  bool registered_{false};

  sensor::Sensor *temp_sensor_{nullptr};
  light::LightState *switch_light_{nullptr};

  // Metody pomocnicze (widoczne w .cpp)
  void generate_guid_();
  bool connect_and_register_();  // inicjuje połączenie i wysyła register (nieblokująco)
  bool send_register_();

  void send_value_temp_();
  void send_value_relay_();
  void send_ping_();
  void handle_incoming_();
  void send_packet_(const uint8_t *payload, uint16_t size);

 private:
  // --- state machine i timery ---
  enum class BridgeState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    REGISTERING,
    RUNNING
  };

  BridgeState state_{BridgeState::DISCONNECTED};

  uint32_t connect_start_ms_{0};
  uint32_t register_start_ms_{0};
  uint32_t last_send_{0};
  uint32_t last_ping_{0};
  uint32_t last_reconnect_attempt_{0};

  // --- nieblokujący parser przychodzących pakietów ---
  SuplaPacketHeader pending_header_{};
  bool pending_header_valid_{false};
  uint16_t pending_payload_size_{0};
  static constexpr size_t PENDING_PAYLOAD_MAX = 1024;
  uint8_t pending_payload_[PENDING_PAYLOAD_MAX]{};

  // Pomocnicze metody
  void start_connect_();
  void process_payload_(uint16_t type, const uint8_t *buf, uint16_t size);
};
}  // namespace supla_esphome_bridge
}  // namespace esphome
