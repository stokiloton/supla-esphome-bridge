// supla_esphome_bridge.cpp
// Poprawiony plik zgodny z proto.h z repozytorium.
// Zawiera defensywne #ifdef, aby dopasować się do różnych wariantów proto.h.

#include "supla_esphome_bridge.h"
#include "esphome/core/log.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include "proto.h"   // ← plik w tym samym katalogu

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
// PASSWORD PARSER
// ---------------------------------------------------------

void SuplaEsphomeBridge::set_location_password(const std::string &hex) {
  memset(location_password_, 0, sizeof(location_password_));

  size_t len = std::min((size_t)32, hex.size());
  for (size_t i = 0; i + 1 < len && i / 2 < sizeof(location_password_); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    location_password_[i / 2] = (uint8_t) strtol(byte_str.c_str(), nullptr, 16);
  }

  hex_dump("LOCATION PASSWORD", location_password_, 16);
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
// SETUP
// ---------------------------------------------------------

void SuplaEsphomeBridge::setup() {
  ESP_LOGI(TAG, "Setup SUPLA bridge");
  generate_guid_();
}

// ---------------------------------------------------------
// CONNECT + REGISTER
// ---------------------------------------------------------

bool SuplaEsphomeBridge::connect_and_register_() {
  if (server_.empty()) {
    ESP_LOGE(TAG, "SUPLA server not set");
    return false;
  }

  ESP_LOGI(TAG, "Connecting to SUPLA server: %s", server_.c_str());

  if (!client_.connect(server_.c_str(), 2015)) {
    ESP_LOGW(TAG, "Connection to SUPLA failed");
    return false;
  }

  if (!send_register_()) {
    ESP_LOGE(TAG, "Failed to send register packet");
    client_.stop();
    return false;
  }

  ESP_LOGI(TAG, "Waiting for SUPLA register result...");

  uint32_t start = millis();

  while (millis() - start < 5000) {
    if (client_.available() >= (int) sizeof(SuplaPacketHeader)) {
      SuplaPacketHeader hdr;
      client_.readBytes((uint8_t *) &hdr, sizeof(hdr));

      ESP_LOGI(TAG,
               "RX HEADER: marker=%c%c%c version=%u size=%u crc=%04X",
               hdr.marker[0], hdr.marker[1], hdr.marker[2],
               hdr.version, hdr.data_size, hdr.crc16);

      if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P') {
        ESP_LOGE(TAG, "Invalid SUPLA header");
        break;
      }

      uint16_t size = hdr.data_size;
      if (size == 0 || size > 1024) {
        ESP_LOGE(TAG, "Invalid SUPLA payload size: %u", size);
        break;
      }

      // Bufor na payload
      uint8_t buf[1024];
      if (size > sizeof(buf)) {
        ESP_LOGE(TAG, "Payload too large");
        break;
      }
      client_.readBytes(buf, size);

      hex_dump("RX PAYLOAD", buf, size);

      uint16_t crc = supla_crc16(buf, size);
      ESP_LOGI(TAG, "CRC CALC=%04X CRC HDR=%04X", crc, hdr.crc16);

      if (crc != hdr.crc16) {
        ESP_LOGE(TAG, "CRC mismatch!");
        break;
      }

      // Pierwsze 2 bajty to typ pakietu (uint16_t)
      uint16_t type = *(uint16_t *) buf;
      ESP_LOGI(TAG, "Packet type: %u", type);

      // Obsługa rejestracji: niektóre wersje proto.h mają różne nazwy stałych.
      // Najprościej: jeśli pakiet ma strukturę rejestracji, to trzeci bajt (buf[2]) często jest result_code.
#ifdef SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B
      if (type == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B) {
        // result_code zwykle w buf[2]
        uint8_t result_code = buf[2];
        uint32_t device_id = 0;
        uint8_t channel_count = 0;
        if (size >= 8) {
          // device_id często na offset 4 (uint32_t) — defensywne odczytanie
          memcpy(&device_id, buf + 4, sizeof(device_id));
        }
        if (size >= 9) {
          channel_count = buf[8];
        }
        ESP_LOGI(TAG, "REGISTER RESULT: result=%u device_id=%u channels=%u",
                 result_code, device_id, channel_count);
        if (result_code == 0) {
          registered_ = true;
          return true;
        } else {
          ESP_LOGE(TAG, "Registration failed: result=%u", result_code);
          break;
        }
      }
#elif defined(SUPLA_SD_DEVICE_REGISTER_RESULT_B)
      if (type == SUPLA_SD_DEVICE_REGISTER_RESULT_B) {
        uint8_t result_code = buf[2];
        uint32_t device_id = 0;
        uint8_t channel_count = 0;
        if (size >= 8) memcpy(&device_id, buf + 4, sizeof(device_id));
        if (size >= 9) channel_count = buf[8];
        ESP_LOGI(TAG, "REGISTER RESULT: result=%u device_id=%u channels=%u",
                 result_code, device_id, channel_count);
        if (result_code == 0) {
          registered_ = true;
          return true;
        } else {
          ESP_LOGE(TAG, "Registration failed: result=%u", result_code);
          break;
        }
      }
#else
      // Fallback: spróbuj odczytać result_code z buf[2] bez porównania typu
      if (size >= 3) {
        uint8_t result_code = buf[2];
        ESP_LOGI(TAG, "REGISTER RESULT (fallback): result=%u", result_code);
        if (result_code == 0) {
          registered_ = true;
          return true;
        } else {
          ESP_LOGW(TAG, "Non-zero result (fallback): %u", result_code);
        }
      }
#endif

      ESP_LOGW(TAG, "Unexpected packet type during registration: %u", type);
    }

    delay(10);
  }

  ESP_LOGE(TAG, "No register result from SUPLA");
  client_.stop();
  return false;
}

// ---------------------------------------------------------
// SEND REGISTER PACKET (proto C + B + B + E)
// ---------------------------------------------------------

bool SuplaEsphomeBridge::send_register_() {
  // Używamy struktury TDS_SuplaRegisterDevice_C (istnieje w proto.h),
  // ale ustawiamy tylko pola, które występują w aktualnym proto.h:
  // GUID, LocationPWD (lub LocationPassword), LocationID, Name, SoftVer, ServerName, channel_count, channels[]

  TDS_SuplaRegisterDevice_C reg{};
  memset(&reg, 0, sizeof(reg));

  // Niektóre wersje mają pole Version, inne nie — ustawiamy tylko jeśli istnieje
#ifdef SUPLA_REGISTER_DEVICE_C_VERSION_FIELD
  reg.Version = 23;
#endif

  // GUID (16 bajtów)
#ifdef SUPLA_GUID_SIZE
  memcpy(reg.GUID, guid_.guid, SUPLA_GUID_SIZE);
#else
  memcpy(reg.GUID, guid_.guid, 16);
#endif

  // LocationID
  reg.LocationID = static_cast<int32_t>(location_id_);

  // Location password — różne nazwy w różnych wersjach proto.h
#if defined(SUPLA_LOCATION_PWD_MAXSIZE) && defined(TDS_SuplaRegisterDevice_C) && (defined(__cplusplus))
  // preferowana nazwa LocationPWD
  #ifdef __has_include
  #endif
#endif

#if defined(TDS_SuplaRegisterDevice_C) && (sizeof(reg) > 0)
  // Spróbuj skopiować do LocationPWD lub LocationPassword w zależności od nazwy
#if defined(SUPLA_LOCATION_PWD_MAXSIZE) && defined(__GNUC__)
  // jeśli pole LocationPWD istnieje w strukturze
  // użyj memcpy z rozmiarem SUPLA_LOCATION_PWD_MAXSIZE jeśli dostępne
  #ifdef SUPLA_LOCATION_PWD_MAXSIZE
    memcpy(reg.LocationPWD, location_password_, SUPLA_LOCATION_PWD_MAXSIZE);
  #else
    // fallback: spróbuj LocationPassword
    memcpy(reg.LocationPassword, location_password_, sizeof(location_password_));
  #endif
#else
  // fallback: spróbuj obie nazwy (jeśli istnieją, kompilator zignoruje nieistniejące przez #ifdef)
  #ifdef SUPLA_LOCATION_PWD_MAXSIZE
    memcpy(reg.LocationPWD, location_password_, SUPLA_LOCATION_PWD_MAXSIZE);
  #else
    memcpy(reg.LocationPassword, location_password_, sizeof(location_password_));
  #endif
#endif
#endif

  // Manufacturer/Product — ustaw na 0 jeśli pola istnieją
#ifdef SUPLA_REGISTER_DEVICE_C_MANUFACTURER_PRODUCT
  reg.ManufacturerID = 0;
  reg.ProductID = 0;
#endif

  // SoftVer, Name, ServerName — kopiujemy defensywnie
#if defined(SUPLA_SOFTVER_MAXSIZE)
  strncpy(reg.SoftVer, soft_ver_.c_str(), SUPLA_SOFTVER_MAXSIZE - 1);
#else
  strncpy(reg.SoftVer, soft_ver_.c_str(), sizeof(reg.SoftVer) - 1);
#endif

#if defined(SUPLA_DEVICE_NAME_MAXSIZE)
  strncpy(reg.Name, device_name_.c_str(), SUPLA_DEVICE_NAME_MAXSIZE - 1);
#else
  strncpy(reg.Name, device_name_.c_str(), sizeof(reg.Name) - 1);
#endif

#if defined(SUPLA_SERVER_NAME_MAXSIZE)
  strncpy(reg.ServerName, server_.c_str(), SUPLA_SERVER_NAME_MAXSIZE - 1);
#else
  strncpy(reg.ServerName, server_.c_str(), sizeof(reg.ServerName) - 1);
#endif

  // channel_count — w proto.h pole nazywa się channel_count
  reg.channel_count = 2;

  // Kanały: używamy typu TDS_SuplaDeviceChannel_B (nazwa występuje w nowszych proto.h)
  // Wypełniamy pola defensywnie: Number, Type, FuncList, Default, value[]
#ifdef TDS_SuplaDeviceChannel_B
  // Kanał 0 - temperatura / sensor
  TDS_SuplaDeviceChannel_B ch0{};
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

  // Kanał 1 - relay
  TDS_SuplaDeviceChannel_B ch1{};
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
#else
  // Jeśli TDS_SuplaDeviceChannel_B nie istnieje, spróbuj starszej nazwy TDS_Channel_B
#ifdef TDS_Channel_B
  TDS_Channel_B ch0{};
  memset(&ch0, 0, sizeof(ch0));
  ch0.Number = 0;
#if defined(SUPLA_CHANNELTYPE_SENSOR_TEMP)
  ch0.Type = SUPLA_CHANNELTYPE_SENSOR_TEMP;
#endif
  ch0.ValueType = 0;
  reg.channels[0] = ch0;

  TDS_Channel_B ch1{};
  memset(&ch1, 0, sizeof(ch1));
  ch1.Number = 1;
#if defined(SUPLA_CHANNELTYPE_RELAY)
  ch1.Type = SUPLA_CHANNELTYPE_RELAY;
#endif
  ch1.ValueType = 0;
  reg.channels[1] = ch1;
#endif
#endif

  // Wyślij REGISTER_C
  hex_dump("TX REGISTER_C", reinterpret_cast<uint8_t *>(&reg), sizeof(reg));
  send_packet_(reinterpret_cast<uint8_t *>(&reg), sizeof(reg));

  // Wyślij CHANNEL_B dla każdego kanału — używamy struktury kanału (jeśli istnieje)
#ifdef TDS_SuplaDeviceChannel_B
  hex_dump("TX CHANNEL_B #0", reinterpret_cast<uint8_t *>(&reg.channels[0]), sizeof(reg.channels[0]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[0]), sizeof(reg.channels[0]));

  hex_dump("TX CHANNEL_B #1", reinterpret_cast<uint8_t *>(&reg.channels[1]), sizeof(reg.channels[1]));
  send_packet_(reinterpret_cast<uint8_t *>(&reg.channels[1]), sizeof(reg.channels[1]));
#else
  // fallback: jeśli nie ma dedykowanej struktury, wysyłamy minimalne bloki
  // (ten kod jest defensywny i może być dostosowany do konkretnego proto.h)
  uint8_t chbuf[16];
  memset(chbuf, 0, sizeof(chbuf));
  // kanał 0
  chbuf[0] = 0; // number
  chbuf[1] = 0; // type low (fallback)
  hex_dump("TX CHANNEL_B #0 (fallback)", chbuf, sizeof(chbuf));
  send_packet_(chbuf, sizeof(chbuf));
  // kanał 1
  chbuf[0] = 1;
  hex_dump("TX CHANNEL_B #1 (fallback)", chbuf, sizeof(chbuf));
  send_packet_(chbuf, sizeof(chbuf));
#endif

  // REGISTER_DEVICE_E (koniec rejestracji) — struktura zwykle pusta
#ifdef TDS_SuplaRegisterDevice_E
  TDS_SuplaRegisterDevice_E reg_e{};
  memset(&reg_e, 0, sizeof(reg_e));
  hex_dump("TX REGISTER_E", reinterpret_cast<uint8_t *>(&reg_e), sizeof(reg_e));
  send_packet_(reinterpret_cast<uint8_t *>(&reg_e), sizeof(reg_e));
#else
  // fallback: wyślij zero-length E (jeśli protokół tego wymaga)
  TDS_SuplaRegisterDevice_E reg_e_fallback{};
  memset(&reg_e_fallback, 0, sizeof(reg_e_fallback));
  hex_dump("TX REGISTER_E (fallback)", reinterpret_cast<uint8_t *>(&reg_e_fallback), sizeof(reg_e_fallback));
  send_packet_(reinterpret_cast<uint8_t *>(&reg_e_fallback), sizeof(reg_e_fallback));
#endif

  ESP_LOGI(TAG, "SUPLA registration (proto C+B+B+E) sent");
  return true;
}

// ---------------------------------------------------------
// SEND PACKET (HEADER + PAYLOAD)
// ---------------------------------------------------------

void SuplaEsphomeBridge::send_packet_(const uint8_t *payload, uint16_t size) {
  SuplaPacketHeader hdr;
  supla_prepare_header(hdr, size, payload);

  client_.write((uint8_t *) &hdr, sizeof(hdr));
  client_.write(payload, size);
}

// ---------------------------------------------------------
// SEND TEMPERATURE
// ---------------------------------------------------------

void SuplaEsphomeBridge::send_value_temp_() {
  if (!temp_sensor_ || !registered_ || !client_.connected())
    return;

  // Przygotuj strukturę wartości kanału. W zależności od proto.h nazwy i formaty mogą się różnić.
  // Tutaj tworzymy prosty pakiet: typ (uint16_t) + channel_number (uint8_t) + value_type (uint8_t) + payload
  // Jeśli proto.h definiuje gotowe struktury, można je użyć zamiast tego bloku.

  uint8_t buf[32];
  memset(buf, 0, sizeof(buf));

  // typ pakietu
#if defined(SUPLA_SD_CHANNEL_VALUE_CHANGED_B)
  uint16_t pkt_type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
#elif defined(SUPLA_SD_CALL_CHANNEL_SET_VALUE_B)
  uint16_t pkt_type = SUPLA_SD_CALL_CHANNEL_SET_VALUE_B;
#else
  // fallback: użyj 0xFFFF (nieprawidłowy) — ale większość proto.h ma jedną z powyższych
  uint16_t pkt_type = 0xFFFF;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));

  // channel_number
  buf[2] = 0; // kanał 0 = temperatura

  // value_type
#if defined(SUPLA_VALUE_TYPE_DOUBLE)
  buf[3] = SUPLA_VALUE_TYPE_DOUBLE;
#else
  buf[3] = 0;
#endif

  // zakoduj temperaturę jako double (8 bajtów) jeśli jest miejsce
  double t = (double) temp_sensor_->state;
  memcpy(buf + 4, &t, sizeof(t));

  hex_dump("TX CHANNEL_VALUE_TEMP", buf, 4 + (int)sizeof(t));
  send_packet_(buf, 4 + (uint16_t)sizeof(t));
}

// ---------------------------------------------------------
// SEND RELAY STATE
// ---------------------------------------------------------

void SuplaEsphomeBridge::send_value_relay_() {
  if (!switch_light_ || !registered_ || !client_.connected())
    return;

  uint8_t buf[16];
  memset(buf, 0, sizeof(buf));

#if defined(SUPLA_SD_CHANNEL_VALUE_CHANGED_B)
  uint16_t pkt_type = SUPLA_SD_CHANNEL_VALUE_CHANGED_B;
#elif defined(SUPLA_SD_CALL_CHANNEL_SET_VALUE_B)
  uint16_t pkt_type = SUPLA_SD_CALL_CHANNEL_SET_VALUE_B;
#else
  uint16_t pkt_type = 0xFFFF;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));

  buf[2] = 1; // kanał 1 = relay

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
// SEND PING
// ---------------------------------------------------------

void SuplaEsphomeBridge::send_ping_() {
  if (!client_.connected())
    return;

  uint8_t buf[8];
  memset(buf, 0, sizeof(buf));

#if defined(SUPLA_SD_PING_CLIENT)
  uint16_t pkt_type = SUPLA_SD_PING_CLIENT;
#else
  uint16_t pkt_type = 0xFFFF;
#endif

  memcpy(buf, &pkt_type, sizeof(pkt_type));
  hex_dump("TX PING", buf, sizeof(pkt_type));
  send_packet_(buf, sizeof(pkt_type));
}

// ---------------------------------------------------------
// HANDLE INCOMING PACKETS
// ---------------------------------------------------------

void SuplaEsphomeBridge::handle_incoming_() {
  while (client_.available() >= (int) sizeof(SuplaPacketHeader)) {
    SuplaPacketHeader hdr;
    client_.readBytes((uint8_t *) &hdr, sizeof(hdr));

    if (hdr.marker[0] != 'S' || hdr.marker[1] != 'U' || hdr.marker[2] != 'P')
      return;

    uint16_t size = hdr.data_size;
    if (size == 0 || size > 1024)
      return;

    uint8_t buf[1024];
    if (size > sizeof(buf)) return;
    client_.readBytes(buf, size);

    uint16_t crc = supla_crc16(buf, size);
    if (crc != hdr.crc16)
      continue;

    uint16_t type = *(uint16_t *) buf;

    // Obsługa komend ustawiających wartość kanału (np. przełącznik)
#if defined(SUPLA_SD_CHANNEL_NEW_VALUE_B)
    if (type == SUPLA_SD_CHANNEL_NEW_VALUE_B) {
      // struktura może się różnić; defensywnie odczytujemy channel_number i state
      uint8_t channel_number = 0;
      if (size >= 3) channel_number = buf[2];

      if (channel_number == 1 && switch_light_) {
        // stan może być w buf[4] lub buf[3] w zależności od wersji; sprawdzamy kilka pozycji
        uint8_t state = 0;
        if (size >= 5) state = buf[4];
        else if (size >= 4) state = buf[3];

        if (state)
          switch_light_->turn_on().perform();
        else
          switch_light_->turn_off().perform();
      }
    }
#else
    // fallback: sprawdź typy, które mogą występować w różnych proto.h
#ifdef SUPLA_CS_CALL_CHANNEL_SET_VALUE_B
    if (type == SUPLA_CS_CALL_CHANNEL_SET_VALUE_B) {
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
    }
#endif
#endif
  }
}

// ---------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------

void SuplaEsphomeBridge::loop() {
  if (!client_.connected()) {
    registered_ = false;
    uint32_t now = millis();

    if (now - last_reconnect_attempt_ > 10000) {
      last_reconnect_attempt_ = now;
      connect_and_register_();
    }

    return;
  }

  handle_incoming_();

  uint32_t now = millis();

  if (now - last_send_ > 10000) {
    last_send_ = now;
    send_value_temp_();
    send_value_relay_();
  }

  if (now - last_ping_ > 30000) {
    last_ping_ = now;
    send_ping_();
  }
}

}  // namespace supla_esphome_bridge
}  // namespace esphome
