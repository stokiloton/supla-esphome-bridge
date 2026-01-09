// supla_esphome_bridge.cpp
// Zmodyfikowany plik: domyślny port TLS = 443, pełne logowanie wszystkiego wysyłanego i odbieranego (hex dump),
// diagnostyki: free heap, CRC check, chunked send_packet_ z timeoutami.
// Uwaga: pełne logowanie dużych pakietów może znacząco obciążyć urządzenie i spowodować opóźnienia.

#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecureBearSSL.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <esp_system.h>
  #include <WiFiClientSecure.h>
#else
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#endif

#include "proto.h"
#include "supla_suml.h"

#include <cstring>
#include <cstdint>

namespace esphome {
namespace supla_esphome_bridge {

static const char *const TAG = "supla_esphome_bridge";

// Pełny hex dump (używać ostrożnie — loguje cały bufor)
static void hex_dump_full(const char *prefix, const uint8_t *data, int len) {
  ESP_LOGI(TAG, "%s (%d bytes):", prefix, len);
  char line[128];
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

  hex_dump_full("LOCATION PASSWORD", location_password_, (int)sizeof(location_password_));
}

// ---------------------------------------------------------
// generate_guid_
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

  hex_dump_full("GUID", guid_.guid, 16);
}

// ---------------------------------------------------------
// init_tls_client_
// ---------------------------------------------------------
void SuplaEsphomeBridge::init_tls_client_() {
#if defined(ESP8266)
  #ifdef USE_SSL_INSECURE
    ESP_LOGW(TAG, "TLS: using INSECURE mode (accept any cert)");
    client_.setInsecure();
  #else
    #if defined(SUPLA_ROOT_CA_PEM) && (SUPLA_ROOT_CA_PEM != nullptr)
      ESP_LOGI(TAG, "TLS: loading root CA (BearSSL)");
      BearSSL::X509List *cert = new BearSSL::X509List(SUPLA_ROOT_CA_PEM);
      client_.setTrustAnchors(cert);
    #else
      ESP_LOGW(TAG, "TLS: no root CA provided, using setInsecure()");
      client_.setInsecure();
    #endif
  #endif
#elif defined(ESP32)
  #ifdef USE_SSL_INSECURE
    ESP_LOGW(TAG, "TLS: using INSECURE mode (accept any cert)");
    client_.setInsecure();
  #else
    #if defined(SUPLA_ROOT_CA_PEM) && (SUPLA_ROOT_CA_PEM != nullptr)
      ESP_LOGI(TAG, "TLS: loading root CA (ESP32)");
      client_.setCACert(SUPLA_ROOT_CA_PEM);
    #else
      ESP_LOGW(TAG, "TLS: no root CA provided, using setInsecure()");
      client_.setInsecure();
    #endif
  #endif
#else
  #ifdef USE_SSL_INSECURE
    client_.setInsecure();
  #else
    client_.setInsecure();
  #endif
#endif
}

// ---------------------------------------------------------
// setup
// ---------------------------------------------------------
void SuplaEsphomeBridge::setup() {
  ESP_LOGI(TAG, "Setup SUPLA bridge (TLS aware)");
  generate_guid_();
  init_tls_client_();

  // Log reset reason (ESP32 only). For other platforms log generic info.
#if defined(ESP32)
  esp_reset_reason_t rr = esp_reset_reason();
  ESP_LOGI(TAG, "Reset reason (esp_reset_reason) = %d", (int)rr);
#else
  ESP_LOGI(TAG, "Reset reason: not logged on this platform");
#endif

  // inicjalizacja timerów i stanu
  if (state_ == BridgeState::DISCONNECTED) {
    connect_start_ms_ = millis();
  }
  last_send_ = millis();
  last_ping_ = millis();
  last_reconnect_attempt_ = 0;

  // Log free heap at startup
#if defined(ESP32) || defined(ESP8266)
  ESP_LOGI(TAG, "Free heap at setup: %u", ESP.getFreeHeap());
#endif
}

// ---------------------------------------------------------
// start_connect_
// ---------------------------------------------------------
void SuplaEsphomeBridge::start_connect_() {
  if (server_.empty()) {
    ESP_LOGE(TAG, "SUPLA server not set");
    return;
  }

  ESP_LOGI(TAG, "Starting TLS connection to SUPLA server: %s", server_.c_str());
  client_.stop();
  connect_start_ms_ = millis();
  state_ = BridgeState::CONNECTING;

  // Domyślny port TLS ustawiony na 443 zgodnie z życzeniem
  const uint16_t tls_port = 443;

  if (client_.connect(server_.c_str(), tls_port)) {
    ESP_LOGI(TAG, "TLS TCP connect ok, sending register");
    state_ = BridgeState::REGISTERING;
    register_start_ms_ = millis();
    send_register_();
  } else {
    ESP_LOGW(TAG, "TLS connect returned false (will retry)");
    state_ = BridgeState::DISCONNECTED;
    connect_start_ms_ = millis();
  }
}

// ---------------------------------------------------------
// connect_and_register_
// ---------------------------------------------------------
bool SuplaEsphomeBridge::connect_and_register_() {
  if (state_ != BridgeState::DISCONNECTED) return false;
  start_connect_();
  return (state_ == BridgeState::REGISTERING);
}

// ---------------------------------------------------------
// send_packet_ (log header, chunked send header+payload, diagnostics)
// ---------------------------------------------------------
void SuplaEsphomeBridge::send_packet_(const uint8_t *payload, uint16_t size) {
  if (!client_.connected()) {
    ESP_LOGW(TAG, "send_packet_: client not connected");
    return;
  }

  SuplaPacketHeader hdr;
  supla_prepare_header(hdr, size, payload);

  // Diagnostics: free heap and CRC check
#if defined(ESP32) || defined(ESP8266)
  ESP_LOGI(TAG, "send_packet_: hdr.data_size=%u hdr.crc=0x%04X free_heap_before=%u",
           (unsigned)hdr.data_size, (unsigned)hdr.crc16, ESP.getFreeHeap());
#else
  ESP_LOGI(TAG, "send_packet_: hdr.data_size=%u hdr.crc=0x%04X",
           (unsigned)hdr.data_size, (unsigned)hdr.crc16);
#endif

  uint16_t crc_local = supla_crc16(payload, size);
  if (crc_local != hdr.crc16) {
    ESP_LOGW(TAG, "CRC mismatch BEFORE send: hdr_crc=0x%04X calc_crc=0x%04X", hdr.crc16, crc_local);
  }

  // Log full header bytes
  hex_dump_full("TX SUPLA_HDR_BYTES", reinterpret_cast<const uint8_t *>(&hdr), (int)sizeof(hdr));
  // Log full payload bytes
  hex_dump_full("TX SUPLA_PAYLOAD", payload, size);

  // Chunked send: header then payload
  const uint8_t *parts[2] = { reinterpret_cast<const uint8_t *>(&hdr), payload };
  const size_t parts_size[2] = { sizeof(hdr), size };
  const unsigned long start = millis();

  for (int p = 0; p < 2; ++p) {
    size_t remaining = parts_size[p];
    const uint8_t *ptr = parts[p];
    while (remaining > 0) {
      size_t chunk = remaining > 128 ? 128 : remaining;

#if defined(ESP8266)
      int avail = client_.availableForWrite();
      if (avail <= 0) {
        if (millis() - start > 1000) {
          ESP_LOGW(TAG, "send_packet_: write timeout (no buffer)");
          client_.stop();
          return;
        }
        delay(1);
        continue;
      }
      if ((size_t)avail < chunk) chunk = avail;
#endif

      int written = client_.write(ptr, chunk);
      if (written <= 0) {
        ESP_LOGW(TAG, "send_packet_: write returned %d", written);
        client_.stop();
        return;
      }
      remaining -= written;
      ptr += written;

      // yield to avoid watchdog
      delay(0);

      if (millis() - start > 2000) {
        ESP_LOGW(TAG, "send_packet_: overall write timeout");
        client_.stop();
        return;
      }
    }
  }

#if defined(ESP32) || defined(ESP8266)
  ESP_LOGI(TAG, "send_packet_ done, elapsed=%lums free_heap_after=%u", millis() - start, ESP.getFreeHeap());
#else
  ESP_LOGI(TAG, "send_packet_ done, elapsed=%lums", millis() - start);
#endif
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

#if defined(SUPLA_GUID_SIZE)
  memcpy(reg.GUID, guid_.guid, SUPLA_GUID_SIZE);
#else
  memcpy(reg.GUID, guid_.guid, 16);
#endif

  reg.LocationID = static_cast<int32_t>(location_id_);

#if defined(SUPLA_LOCATION_PWD_MAXSIZE)
  memcpy(reg.LocationPWD, location_password_, SUPLA_LOCATION_PWD_MAXSIZE);
#else
  #ifdef TDS_SuplaRegisterDevice_C__HAS_LocationPassword
    memcpy(reg.LocationPassword, location_password_, sizeof(location_password_));
  #endif
#endif

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

  // defensywnie ustaw channel_count jeśli pole istnieje
  #ifdef HAS_CHANNEL_COUNT_FIELD
    reg.channel_count = 2;
  #else
    #ifdef HAS_ChannelCount_FIELD
      reg.ChannelCount = 2;
    #endif
  #endif

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

  // Log i wysyłka REGISTER_C (pełny dump)
  hex_dump_full("TX REGISTER_C", reinterpret_cast<uint8_t *>(&reg), (int)sizeof(reg));
  send_packet_(reinterpret_cast<uint8_t *>(&reg), (uint16_t)sizeof(reg));

#if defined(TDS_SuplaDeviceChannel_B)
  hex_dump_full("TX CHANNEL_B #0", reinterpret_cast<uint8_t *>(&reg.channels[0]), (int)sizeof(reg.channels[0]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[0]), (uint16_t)sizeof(reg.channels[0]));

  hex_dump_full("TX CHANNEL_B #1", reinterpret_cast<uint8_t *>(&reg.channels[1]), (int)sizeof(reg.channels[1]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[1]), (uint16_t)sizeof(reg.channels[1]));
#else
  uint8_t chbuf[16];
  memset(chbuf, 0, sizeof(chbuf));
  chbuf[0] = 0;
  hex_dump_full("TX CHANNEL_B #0 (fallback)", chbuf, (int)sizeof(chbuf));
  send_packet_(chbuf, (uint16_t)sizeof(chbuf));
  chbuf[0] = 1;
  hex_dump_full("TX CHANNEL_B #1 (fallback)", chbuf, (int)sizeof(chbuf));
  send_packet_(chbuf, (uint16_t)sizeof(chbuf));
#endif

#if defined(TDS_SuplaRegisterDevice_E)
  TDS_SuplaRegisterDevice_E reg_e;
  memset(&reg_e, 0, sizeof(reg_e));
  hex_dump_full("TX REGISTER_E", reinterpret_cast<uint8_t *>(&reg_e), (int)sizeof(reg_e));
  send_packet_(reinterpret_cast<uint8_t *>(&reg_e), (uint16_t)sizeof(reg_e));
#endif

  ESP_LOGI(TAG, "SUPLA registration sent (TLS)");
  return true;
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

  hex_dump_full("TX CHANNEL_VALUE_TEMP", buf, (int)(4 + sizeof(t)));
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

  hex_dump_full("TX CHANNEL_VALUE_RELAY", buf, 5);
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
  hex_dump_full("TX PING", buf, sizeof(pkt_type));
  send_packet_(buf, sizeof(pkt_type));
}

// ---------------------------------------------------------
// process_payload_
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
// handle_incoming_ (nieblokujące, z pełnym logowaniem nagłówka i payloadu)
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
        ESP_LOGE(TAG, "CRC mismatch on pending payload (hdr_crc=0x%04X calc=0x%04X)", pending_header_.crc16, crc);
        client_.stop();
        state_ = BridgeState::DISCONNECTED;
        registered_ = false;
        pending_header_valid_ = false;
        return;
      }

      // Log received payload fully
      hex_dump_full("RX SUPLA_PAYLOAD (pending)", pending_payload_, need);

      uint16_t type = 0;
      if (need >= 2) memcpy(&type, pending_payload_, sizeof(type));
      process_payload_(type, pending_payload_, need);
      pending_header_valid_ = false;
    }
    return;
  }

  // Brak pending header: odczytuj header tylko jeśli dostępne co najmniej sizeof(header)
  while (client_.available() >= (int)sizeof(SuplaPacketHeader)) {
    SuplaPacketHeader hdr;
    int r = client_.read((uint8_t *)&hdr, sizeof(hdr));
    if (r != (int)sizeof(hdr)) {
      ESP_LOGW(TAG, "Partial header read (%d)", r);
      return;
    }

    // Log nagłówka przy odbiorze (pełny dump nagłówka)
    hex_dump_full("RX SUPLA_HDR_BYTES", reinterpret_cast<const uint8_t *>(&hdr), (int)sizeof(hdr));
    ESP_LOGI(TAG, "RX SUPLA HDR: marker=%c%c%c data_size=%u crc=0x%04X available=%d",
             hdr.marker[0], hdr.marker[1], hdr.marker[2],
             (unsigned)hdr.data_size, (unsigned)hdr.crc16, client_.available());

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

    // Jeśli cały payload jest już dostępny, odczytaj go
    if (client_.available() >= (int)size) {
      client_.read(pending_payload_, size);
      uint16_t crc = supla_crc16(pending_payload_, size);
      if (crc != hdr.crc16) {
        ESP_LOGE(TAG, "CRC mismatch (hdr=0x%04X calc=0x%04X)", hdr.crc16, crc);
        client_.stop();
        state_ = BridgeState::DISCONNECTED;
        registered_ = false;
        return;
      }

      // Log received payload fully
      hex_dump_full("RX SUPLA_PAYLOAD", pending_payload_, size);

      uint16_t type = 0;
      if (size >= 2) memcpy(&type, pending_payload_, sizeof(type));
      process_payload_(type, pending_payload_, size);
      continue;
    } else {
      // payload nie jest jeszcze w całości dostępny -> zapisz header i poczekaj
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
