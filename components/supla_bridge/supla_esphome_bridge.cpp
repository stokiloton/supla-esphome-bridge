// supla_esphome_bridge.cpp
#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include "proto.h"
#include "supla_suml.h"

#include <cstring>
#include <cstdint>

namespace esphome {
namespace supla_esphome_bridge {

static const char *const TAG = "supla_esphome_bridge";

// ---------------------------------------------------------
// HEX DUMP
// ---------------------------------------------------------
static void hex_dump(const char *prefix, const uint8_t *data, int len) {
  ESP_LOGI(TAG, "%s (%d bytes):", prefix, len);
  char line[80];

  for (int i = 0; i < len; i += 16) {
    int pos = 0;
    pos += sprintf(line + pos, "%04X: ", i);

    for (int j = 0; j < 16 && i + j < len; j++)
      pos += sprintf(line + pos, "%02X ", data[i + j]);

    ESP_LOGI(TAG, "%s", line);
  }
}

// ---------------------------------------------------------
// set_location_password
// ---------------------------------------------------------
void SuplaEsphomeBridge::set_location_password(const std::string &hex) {
  memset(location_password_, 0, sizeof(location_password_));

  size_t len = std::min((size_t)32, hex.size());
  for (size_t i = 0; i + 1 < len && (i / 2) < sizeof(location_password_); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    location_password_[i / 2] = (uint8_t) strtol(byte_str.c_str(), nullptr, 16);
  }

  hex_dump("LOCATION PASSWORD", location_password_, (int)sizeof(location_password_));
}

// ---------------------------------------------------------
// GUID GENERATOR
// ---------------------------------------------------------
void SuplaEsphomeBridge::generate_guid_() {
  uint8_t mac[6];

#if defined(ESP8266) || defined(ESP32)
  WiFi.macAddress(mac);
#else
  for (int i = 0; i < 6; i++)
    mac[i] = i * 11;
#endif

  memset(guid_.guid, 0, sizeof(guid_.guid));
  memcpy(guid_.guid, mac, 6);

  uint32_t t = millis();
  memcpy(guid_.guid + 6, &t, sizeof(t));

  hex_dump("GUID", guid_.guid, 16);
}

// ---------------------------------------------------------
// setup
// ---------------------------------------------------------
void SuplaEsphomeBridge::setup() {
  ESP_LOGI(TAG, "Setup SUPLA bridge");
  generate_guid_();

  // inicjalizacja timerów i stanu
  if (state_ == BridgeState::DISCONNECTED) {
    connect_start_ms_ = millis();
  }
  last_send_ = millis();
  last_ping_ = millis();
  last_reconnect_attempt_ = 0;
}

// ---------------------------------------------------------
// start_connect_ (inicjuje połączenie, nie blokuje oczekiwania na odpowiedź)
// ---------------------------------------------------------
void SuplaEsphomeBridge::start_connect_() {
  if (server_.empty()) {
    ESP_LOGE(TAG, "SUPLA server not set");
    return;
  }

  ESP_LOGI(TAG, "Starting connection to SUPLA server: %s", server_.c_str());
  client_.stop();
  connect_start_ms_ = millis();
  state_ = BridgeState::CONNECTING;

  // WiFiClient::connect zwykle zwraca szybko; jeśli zwróci true, przechodzimy dalej
  if (client_.connect(server_.c_str(), 2015)) {
    ESP_LOGI(TAG, "TCP connect ok, sending register");
    state_ = BridgeState::REGISTERING;
    register_start_ms_ = millis();
    // Wyślij rejestrację asynchronicznie
    send_register_();
  } else {
    ESP_LOGW(TAG, "TCP connect returned false (will retry)");
    state_ = BridgeState::DISCONNECTED;
    connect_start_ms_ = millis();
  }
}

// ---------------------------------------------------------
// connect_and_register_ (publiczna metoda kompatybilności)
// ---------------------------------------------------------
bool SuplaEsphomeBridge::connect_and_register_() {
  if (state_ != BridgeState::DISCONNECTED) return false;
  start_connect_();
  return (state_ == BridgeState::REGISTERING);
}

// ---------------------------------------------------------
// send_register_ (wysyła REGISTER_C + CHANNEL_B + CHANNEL_B + REGISTER_E)
// ---------------------------------------------------------
bool SuplaEsphomeBridge::send_register_() {
  if (!client_.connected()) {
    ESP_LOGW(TAG, "send_register_: client not connected");
    return false;
  }

  TDS_SuplaRegisterDevice_C reg;
  memset(&reg, 0, sizeof(reg));

  // GUID
#if defined(SUPLA_GUID_SIZE)
  memcpy(reg.GUID, guid_.guid, SUPLA_GUID_SIZE);
#else
  memcpy(reg.GUID, guid_.guid, 16);
#endif

  // LocationID
  reg.LocationID = static_cast<int32_t>(location_id_);

  // Location password - różne nazwy w różnych proto.h
#if defined(SUPLA_LOCATION_PWD_MAXSIZE)
  memcpy(reg.LocationPWD, location_password_, SUPLA_LOCATION_PWD_MAXSIZE);
#else
  // fallback: spróbuj skopiować do LocationPassword jeśli istnieje
  #ifdef TDS_SuplaRegisterDevice_C__HAS_LocationPassword
    memcpy(reg.LocationPassword, location_password_, sizeof(location_password_));
  #endif
#endif

  // SoftVer i Name
#if defined(SUPLA_SOFTVER_MAXSIZE)
  strncpy(reg.SoftVer, "1.0", SUPLA_SOFTVER_MAXSIZE - 1);
#else
  strncpy(reg.SoftVer, "1.0", sizeof(reg.SoftVer) - 1);
#endif

#if defined(SUPLA_DEVICE_NAME_MAXSIZE)
  strncpy(reg.Name, device_name_.c_str(), SUPLA_DEVICE_NAME_MAXSIZE - 1);
#else
  strncpy(reg.Name, device_name_.c_str(), sizeof(reg.Name) - 1);
#endif

#if defined(SUPLA_SERVER_NAME_MAXSIZE)
  strncpy(reg.ServerName, server_.c_str(), SUPLA_SERVER_NAME_MAXSIZE - 1);
#else
  #ifdef TDS_SuplaRegisterDevice_C__HAS_ServerName
    strncpy(reg.ServerName, server_.c_str(), sizeof(reg.ServerName) - 1);
  #endif
#endif

  // channel_count (nowsze proto.h)
  reg.channel_count = 2;

  // Przygotuj kanały (najczęściej TDS_SuplaDeviceChannel_B)
#if defined(TDS_SuplaDeviceChannel_B)
  TDS_SuplaDeviceChannel_B ch0;
  memset(&ch0, 0, sizeof(ch0));
  ch0.Number = 0;
#if defined(SUPLA_CHANNELTYPE_THERMOMETER)
  ch0.Type = SUPLA_CHANNELTYPE_THERMOMETER;
#elif defined(SUPLA_CHANNELTYPE_SENSOR_TEMP)
  ch0.Type = SUPLA_CHANNELTYPE_SENSOR_TEMP;
#else
  ch0.Type = 0;
#endif
#if defined(SUPLA_CHANNELFNC_THERMOMETER)
  ch0.FuncList = SUPLA_CHANNELFNC_THERMOMETER;
#endif
  ch0.Default = 0;
  memset(ch0.value, 0, sizeof(ch0.value));
  reg.channels[0] = ch0;

  TDS_SuplaDeviceChannel_B ch1;
  memset(&ch1, 0, sizeof(ch1));
  ch1.Number = 1;
#if defined(SUPLA_CHANNELTYPE_RELAY)
  ch1.Type = SUPLA_CHANNELTYPE_RELAY;
#else
  ch1.Type = 0;
#endif
#if defined(SUPLA_CHANNELFNC_LIGHTSWITCH)
  ch1.FuncList = SUPLA_CHANNELFNC_LIGHTSWITCH;
#endif
  ch1.Default = 0;
  memset(ch1.value, 0, sizeof(ch1.value));
  reg.channels[1] = ch1;
#endif

  // Wyślij REGISTER_C
  hex_dump("TX REGISTER_C", reinterpret_cast<uint8_t *>(&reg), sizeof(reg));
  send_packet_(reinterpret_cast<uint8_t *>(&reg), sizeof(reg));

  // Wyślij CHANNEL_B (jeśli struktury istnieją)
#if defined(TDS_SuplaDeviceChannel_B)
  hex_dump("TX CHANNEL_B #0", reinterpret_cast<uint8_t *>(&reg.channels[0]), sizeof(reg.channels[0]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[0]), sizeof(reg.channels[0]));

  hex_dump("TX CHANNEL_B #1", reinterpret_cast<uint8_t *>(&reg.channels[1]), sizeof(reg.channels[1]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[1]), sizeof(reg.channels[1]));
#else
  // fallback minimalny
  uint8_t chbuf[16];
  memset(chbuf, 0, sizeof(chbuf));
  chbuf[0] = 0;
  hex_dump("TX CHANNEL_B #0 (fallback)", chbuf, sizeof(chbuf));
  send_packet_(chbuf, sizeof(chbuf));
  chbuf[0] = 1;
  hex_dump("TX CHANNEL_B #1 (fallback)", chbuf, sizeof(chbuf));
  send_packet_(chbuf, sizeof(chbuf));
#endif

  // REGISTER_DEVICE_E (koniec rejestracji) - jeśli struktura istnieje
#if defined(TDS_SuplaRegisterDevice_E)
  TDS_SuplaRegisterDevice_E reg_e;
  memset(&reg_e, 0, sizeof(reg_e));
  hex_dump("TX REGISTER_E", reinterpret_cast<uint8_t *>(&reg_e), sizeof(reg_e));
  send_packet_(reinterpret_cast<uint8_t *>(&reg_e), sizeof(reg_e));
#endif

  ESP_LOGI(TAG, "SUPLA registration sent");
  return true;
}

// ---------------------------------------------------------
// send_packet_ (header + payload)
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_packet_(const uint8_t *payload, uint16_t size) {
  if (!client_.connected()) {
    ESP_LOGW(TAG, "send_packet_: client not connected");
    return;
  }

  SuplaPacketHeader hdr;
  supla_prepare_header(hdr, size, payload);

  client_.write((uint8_t *)&hdr, sizeof(hdr));
  client_.write(payload, size);
}

// ---------------------------------------------------------
// send_value_temp_
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_value_temp_() {
  if (!temp_sensor_ || !registered_ || !client_.connected())
    return;

  uint8_t buf[4 + sizeof(double)];
  memset(buf, 0, sizeof(buf));

  uint16_t pkt_type = 0;
#if defined(SUPLA_SD_CHANNEL_VALUE_CHANGED_B)
  pkt_type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
#elif defined(SUPLA_SD_CALL_CHANNEL_SET_VALUE_B)
  pkt_type = SUPLA_SD_CALL_CHANNEL_SET_VALUE_B;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));
  buf[2] = 0; // channel 0
#if defined(SUPLA_VALUE_TYPE_DOUBLE)
  buf[3] = SUPLA_VALUE_TYPE_DOUBLE;
#else
  buf[3] = 0;
#endif

  double t = (double) temp_sensor_->state;
  memcpy(buf + 4, &t, sizeof(t));

  hex_dump("TX CHANNEL_VALUE_TEMP", buf, (int)(4 + sizeof(t)));
  send_packet_(buf, (uint16_t)(4 + sizeof(t)));
}

// ---------------------------------------------------------
// send_value_relay_
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_value_relay_() {
  if (!switch_light_ || !registered_ || !client_.connected())
    return;

  uint8_t buf[8];
  memset(buf, 0, sizeof(buf));

  uint16_t pkt_type = 0;
#if defined(SUPLA_SD_CHANNEL_VALUE_CHANGED_B)
  pkt_type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
#elif defined(SUPLA_SD_CALL_CHANNEL_SET_VALUE_B)
  pkt_type = SUPLA_SD_CALL_CHANNEL_SET_VALUE_B;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));
  buf[2] = 1; // channel 1
#if defined(SUPLA_VALUE_TYPE_ONOFF)
  buf[3] = SUPLA_VALUE_TYPE_ONOFF;
#else
  buf[3] = 0;
#endif

  buf[4] = switch_light_->current_values.is_on() ? 1 : 0;

  hex_dump("TX CHANNEL_VALUE_RELAY", buf, 5);
  send_packet_(buf, 5);
}

// ---------------------------------------------------------
// send_ping_
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_ping_() {
  if (!client_.connected())
    return;

  uint8_t buf[4];
  memset(buf, 0, sizeof(buf));

  uint16_t pkt_type = 0;
#ifdef SUPLA_SD_PING_CLIENT
  pkt_type = SUPLA_SD_PING_CLIENT;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));
  hex_dump("TX PING", buf, sizeof(pkt_type));
  send_packet_(buf, sizeof(pkt_type));
}

// ---------------------------------------------------------
// process_payload_ (metoda klasy)
// ---------------------------------------------------------
void SuplaEsphomeBridge::process_payload_(uint16_t type, const uint8_t *buf, uint16_t size) {
  ESP_LOGI(TAG, "Processing payload type=%u size=%u", type, size);

#if defined(SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B)
  if (type == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B) {
    uint8_t result_code = 0;
    if (size >= 3) result_code = buf[2];
    ESP_LOGI(TAG, "REGISTER RESULT: result=%u", result_code);
    if (result_code == 0) {
      registered_ = true;
      state_ = BridgeState::RUNNING;
      last_send_ = millis();
      last_ping_ = millis();
    } else {
      ESP_LOGE(TAG, "Registration failed: %u", result_code);
      client_.stop();
      state_ = BridgeState::DISCONNECTED;
      connect_start_ms_ = millis();
    }
    return;
  }
#endif

#if defined(SUPLA_SD_DEVICE_REGISTER_RESULT_B)
  if (type == SUPLA_SD_DEVICE_REGISTER_RESULT_B) {
    uint8_t result_code = 0;
    if (size >= 3) result_code = buf[2];
    ESP_LOGI(TAG, "REGISTER RESULT (alt): result=%u", result_code);
    if (result_code == 0) {
      registered_ = true;
      state_ = BridgeState::RUNNING;
      last_send_ = millis();
      last_ping_ = millis();
    } else {
      ESP_LOGE(TAG, "Registration failed (alt): %u", result_code);
      client_.stop();
      state_ = BridgeState::DISCONNECTED;
      connect_start_ms_ = millis();
    }
    return;
  }
#endif

#if defined(SUPLA_SD_CHANNEL_NEW_VALUE_B)
  if (type == SUPLA_SD_CHANNEL_NEW_VALUE_B) {
    uint8_t channel_number = 0;
    if (size >= 3) channel_number = buf[2];
    if (channel_number == 1 && switch_light_) {
      uint8_t state = 0;
      if (size >= 5) state = buf[4];
      else if (size >= 4) state = buf[3];
      if (state)
        switch_light_->turn_on().perform();
      else
        switch_light_->turn_off().perform();
    }
    return;
  }
#endif

  ESP_LOGW(TAG, "Unhandled payload type=%u", type);
}

// ---------------------------------------------------------
// handle_incoming_ (nieblokujące, używa pending buffer)
// ---------------------------------------------------------
void SuplaEsphomeBridge::handle_incoming_() {
  // Jeśli mamy już header i czekamy na payload
  if (pending_header_valid_) {
    uint16_t need = pending_payload_size_;
    if (client_.available() >= (int)need) {
      if (need > (int)PENDING_PAYLOAD_MAX) need = (int)PENDING_PAYLOAD_MAX;
      client_.read(pending_payload_, need);

      uint16_t crc = supla_crc16(pending_payload_, need);
      if (crc != pending_header_.crc16) {
        ESP_LOGE(TAG, "CRC mismatch on pending payload");
        client_.stop();
        state_ = BridgeState::DISCONNECTED;
        registered_ = false;
        pending_header_valid_ = false;
        return;
      }

      uint16_t type = 0;
      if (need >= 2) memcpy(&type, pending_payload_, sizeof(type));
      process_payload_(type, pending_payload_, need);
      pending_header_valid_ = false;
    }
    return;
  }

  // Brak pending header: spróbuj odczytać header tylko jeśli dostępne co najmniej sizeof(header)
  while (client_.available() >= (int)sizeof(SuplaPacketHeader)) {
    SuplaPacketHeader hdr;
    int r = client_.read((uint8_t *)&hdr, sizeof(hdr));
    if (r != (int)sizeof(hdr)) {
      // nie udało się odczytać pełnego headera - przerwij
      return;
    }

    // Walidacja marker
    if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
      ESP_LOGE(TAG, "Invalid SUPLA header marker");
      client_.stop();
      state_ = BridgeState::DISCONNECTED;
      registered_ = false;
      return;
    }

    uint16_t size = hdr.data_size;
    if (size == 0 || size > (int)PENDING_PAYLOAD_MAX) {
      ESP_LOGE(TAG, "Invalid payload size: %u", size);
      client_.stop();
      state_ = BridgeState::DISCONNECTED;
      registered_ = false;
      return;
    }

    // Jeśli cały payload jest już dostępny (header + payload), odczytaj całość
    if (client_.available() >= (int)size) {
      client_.read(pending_payload_, size);

      uint16_t crc = supla_crc16(pending_payload_, size);
      if (crc != hdr.crc16) {
        ESP_LOGE(TAG, "CRC mismatch");
        client_.stop();
        state_ = BridgeState::DISCONNECTED;
        registered_ = false;
        return;
      }

      uint16_t type = 0;
      if (size >= 2) memcpy(&type, pending_payload_, sizeof(type));
      process_payload_(type, pending_payload_, size);
      // kontynuuj pętlę, aby obsłużyć kolejne pakiety
      continue;
    } else {
      // Payload nie jest jeszcze w całości dostępny -> zapisz header i poczekaj
      pending_header_ = hdr;
      pending_payload_size_ = size;
      pending_header_valid_ = true;
      return;
    }
  }
}

// ---------------------------------------------------------
// loop (state-machine, non-blocking)
// ---------------------------------------------------------
void SuplaEsphomeBridge::loop() {
  uint32_t now = millis();

  switch (state_) {
    case BridgeState::DISCONNECTED:
      // próbuj łączyć co 10s
      if (now - connect_start_ms_ > 10000) {
        connect_start_ms_ = now;
        start_connect_();
      }
      break;

    case BridgeState::CONNECTING:
      // jeśli połączenie nie nastąpiło w 8s, wróć do DISCONNECTED
      if (!client_.connected()) {
        if (now - connect_start_ms_ > 8000) {
          ESP_LOGW(TAG, "Connect timeout");
          client_.stop();
          state_ = BridgeState::DISCONNECTED;
          connect_start_ms_ = now;
        }
      } else {
        state_ = BridgeState::REGISTERING;
        register_start_ms_ = now;
        send_register_();
      }
      break;

    case BridgeState::REGISTERING:
      // sprawdzaj przychodzące dane bez blokowania
      handle_incoming_();
      // timeout rejestracji
      if (now - register_start_ms_ > 8000) {
        ESP_LOGW(TAG, "Register timeout, closing connection and retrying");
        client_.stop();
        state_ = BridgeState::DISCONNECTED;
        connect_start_ms_ = now;
      }
      break;

    case BridgeState::RUNNING:
      if (!client_.connected()) {
        ESP_LOGW(TAG, "Lost TCP connection, switching to DISCONNECTED");
        state_ = BridgeState::DISCONNECTED;
        connect_start_ms_ = now;
        registered_ = false;
        return;
      }

      // obsługa przychodzących pakietów
      handle_incoming_();

      // wysyłanie wartości okresowo
      if (now - last_send_ > 10000) {
        last_send_ = now;
        send_value_temp_();
        send_value_relay_();
      }

      // ping
      if (now - last_ping_ > 30000) {
        last_ping_ = now;
        send_ping_();
      }
      break;
  }
}

}  // namespace supla_esphome_bridge
}  // namespace esphome
