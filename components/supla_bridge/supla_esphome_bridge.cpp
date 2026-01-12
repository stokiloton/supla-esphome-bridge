#include "supla_esphome_bridge.h"
#include <cstddef>
#include <cstring>
#include <cstdint>

#ifndef SUPLA_PROTO_VERSION
#define SUPLA_PROTO_VERSION 23
#endif



namespace supla_esphome_bridge {



SuplaEsphomeBridge::SuplaEsphomeBridge() {
  sproto_ctx_ = sproto_init();
  if (sproto_ctx_) {
    sproto_set_version(sproto_ctx_, SUPLA_PROTO_VERSION);
    ESP_LOGI("supla", "sproto initialized, forced proto version=%u",
             (unsigned)SUPLA_PROTO_VERSION);
  } else {
    ESP_LOGW("supla", "sproto_init failed");
  }
}

SuplaEsphomeBridge::~SuplaEsphomeBridge() {
  if (sproto_ctx_) {
    sproto_free(sproto_ctx_);
    sproto_ctx_ = nullptr;
  }
}

void SuplaEsphomeBridge::setup() {
  ESP_LOGI("supla", "SuplaEsphomeBridge setup()");
}

void SuplaEsphomeBridge::loop() {
  static unsigned long last_try = 0;
  if (!registered_ && millis() - last_try > 40000) {
    last_try = millis();
    register_device(3000);
    yield();
    delay(1);
  }
}

void SuplaEsphomeBridge::hex_dump(const uint8_t *buf, size_t len, const char *prefix) {
  const size_t per_line = 16;
  char line[128];
  for (size_t i = 0; i < len; i += per_line) {
    size_t chunk = (i + per_line <= len) ? per_line : (len - i);
    int pos = snprintf(line, sizeof(line), "%s %04X: ", prefix, (unsigned)i);
    for (size_t j = 0; j < chunk; j++)
      pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i + j]);
    pos += snprintf(line + pos, sizeof(line) - pos, " | ");
    for (size_t j = 0; j < chunk; j++) {
      char c = buf[i + j];
      line[pos++] = (c >= 32 && c < 127) ? c : '.';
    }
    line[pos] = 0;
    ESP_LOGD("supla", "%s", line);
  }
}

bool SuplaEsphomeBridge::register_device(unsigned long timeout_ms) {

  if (server_.empty()) {
    ESP_LOGW("supla", "No SUPLA server configured");
    return false;
  }
  
  ESP_LOGI("supla", "client_.setTimeout(1000)");
  client_.setTimeout(3000);
  yield();
  delay(1);
  
  ESP_LOGI("supla", "Connecting to SUPLA server %s:2015", server_.c_str());

  bool ok = false;
  for (int i = 0; i < 50; i++) {
    if (client_.connect(server_.c_str(), 2015)) { 
      ok = true;
      break;
    }
    delay(10);
    yield();
  }
  
  if (!ok) {
    ESP_LOGW("supla", "Cannot connect to SUPLA server");
    return false;
  }
  ESP_LOGI("supla", "Connected to SUPLA server %s:2015", server_.c_str());

  yield();
  delay(1);
  
  const unsigned call_id = SUPLA_DS_CALL_REGISTER_DEVICE_F;
  ESP_LOGI("supla", "Attempting register with call_id=%u (REGISTER_DEVICE_F)", call_id);

  // -------------------------
  // Build TDS_SuplaRegisterDevice_F
  // -------------------------
  static TDS_SuplaRegisterDevice_F reg;
  memset(&reg, 0, sizeof(reg));

  yield();
  delay(1);

  // LocationID / LocationPWD
 // reg.LocationID = location_id_;

 // strncpy(reg.LocationPWD, location_password_.c_str(), SUPLA_LOCATION_PWD_MAXSIZE - 1);
  //reg.LocationPWD[SUPLA_LOCATION_PWD_MAXSIZE - 1] = '\0';

  // EMAIL
  strncpy(reg.Email, "stokiloton@gmail.com", SUPLA_EMAIL_MAXSIZE - 1);
  //reg.Email[SUPLA_EMAIL_MAXSIZE - 1] = '\0';

  // AUTHKEY (16 bytes, stały)
  static const uint8_t AUTHKEY[SUPLA_AUTHKEY_SIZE] = {
    0xA3,0x5F,0x91,0x0C,0x67,0x2B,0x4E,0x88,
    0x19,0xC4,0x5A,0x0D,0x6E,0x3F,0x12,0x99
  };
  memcpy(reg.AuthKey, AUTHKEY, SUPLA_AUTHKEY_SIZE);
  

  // GUID: 1C81FE5A-DDDD-BCD1-FCC1-0F42C159618E
  uint8_t GUID_BIN[SUPLA_GUID_SIZE] = {
    0x1C, 0x81, 0xFE, 0x5A, 0xED, 0xDD, 0xBC, 0xD1,
    0xFC, 0xC1, 0x0F, 0x42, 0xC1, 0x59, 0x61, 0x8E
  };

  // GUID
  memcpy(reg.GUID, GUID_BIN, SUPLA_GUID_SIZE);

  // Name
  strncpy(reg.Name, device_name_.c_str(), SUPLA_DEVICE_NAME_MAXSIZE - 1);
  //reg.Name[SUPLA_DEVICE_NAME_MAXSIZE - 1] = '\0';

  // SoftVer
  strncpy(reg.SoftVer, "GGv1", SUPLA_SOFTVER_MAXSIZE - 1);
  //reg.SoftVer[SUPLA_SOFTVER_MAXSIZE - 1] = '\0';

  // ServerName
  strncpy(reg.ServerName, server_.c_str(), SUPLA_SERVER_NAME_MAXSIZE - 1);
  //reg.ServerName[SUPLA_SERVER_NAME_MAXSIZE - 1] = '\0';
  
  // Flags, ManufacturerID, ProductID
  reg.Flags = 0;
  reg.ManufacturerID = 0;
  reg.ProductID = 0;

  yield();
  delay(1);


  reg.channel_count = 1;

  TDS_SuplaDeviceChannel_D &ch = reg.channels[0];


fill_channel_D(
    ch,
    0,                                  // Number
    SUPLA_CHANNELTYPE_RELAY,            // Type
    SUPLA_CHANNELFNC_POWERSWITCH,       // FuncList
    0,                                  // Default
    0,                                  // Flags
    false,                              // Offline
    0,                                  // ValueValidityTimeSec
    "0",                                // initial value
    0                    // DefaultIcon
);

  
  //memset(&ch, 0, sizeof(ch));

  //ch.Number = 0;
  //ch.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  //ch.FuncList = SUPLA_BIT_FUNC_THERMOMETER;
  //ch.Default = 0;
  
  //ch.Flags = 0;
  //ch.Offline = 0;
  //ch.ValueValidityTimeSec = 0;
  
  //memset(ch.value, 0, SUPLA_CHANNELVALUE_SIZE);
  //ch.DefaultIcon = 0;
  //ch.SubDeviceId = 0;

  // -------------------------
  // Payload size
  // -------------------------
  yield();
  delay(1);

  size_t payload_size =
      offsetof(TDS_SuplaRegisterDevice_F, channels) +
      reg.channel_count * sizeof(TDS_SuplaDeviceChannel_D);

  ESP_LOGI("supla", "Prepared REGISTER_DEVICE_F payload_size=%u (channel_count=%u)",
           (unsigned)payload_size, (unsigned)reg.channel_count);

  yield();
  delay(10);
  
  hex_dump((const uint8_t*)&reg, payload_size, "REG-PAYLOAD");

  yield();
  delay(1);
  
  // -------------------------
  // Build SDP (RAW MODE)
  // -------------------------
  TSuplaDataPacket *sdp = sproto_sdp_malloc(sproto_ctx_);
  if (!sdp) {
    ESP_LOGW("supla", "sproto_sdp_malloc failed");
    client_.stop();
    return false;
  }
  ESP_LOGI("supla", "sproto_sdp_malloc OK");
  
  yield();
  delay(1);
  
  sproto_sdp_init(sproto_ctx_, sdp);
  
  yield();
  delay(1);

  size_t dat_size = sizeof(TDS_SuplaRegisterDevice_F);
  
  ESP_LOGI("supla", "SUPLA_MAX_DATA_SIZE=%u , payload_size=%u, dat_size=%u  ", SUPLA_MAX_DATA_SIZE, payload_size, dat_size);
  
  if (!sproto_set_data(sdp, (char*)&reg, (unsigned _supla_int_t)payload_size, call_id)) {
    ESP_LOGW("supla", "sproto_set_data failed");
    sproto_sdp_free(sdp);
    client_.stop();
    return false;
  }
  ESP_LOGW("supla", "sproto_set_data OK");

  size_t packet_len =
      sizeof(TSuplaDataPacket) - SUPLA_MAX_DATA_SIZE + sdp->data_size;

  ESP_LOGI("supla", "Sending RAW REGISTER_DEVICE_F (call_id=%u), len=%u",
           call_id, (unsigned)packet_len);

  yield();
  delay(1);
  
  hex_dump((uint8_t*)sdp, packet_len, "TX");

  yield();
  delay(1);

    size_t packet_len2 = sizeof(sdp);

  size_t sent = client_.write((uint8_t*)sdp, packet_len2);

  yield();
  delay(1);
  
  client_.flush();

  yield();
  delay(1);
  
  sproto_sdp_free(sdp);

      yield();
    delay(1);

  if (sent != packet_len) {
    ESP_LOGW("supla", "Sent mismatch: %u != %u",
            (unsigned)sent, (unsigned)packet_len);
    client_.stop();
    return false;
  }

  ESP_LOGI("supla", "REGISTER_DEVICE_F sent");

  yield();
  delay(1);
  
  //tymczsow 1 proba
  registered_ = true;

  bool resp = read_register_response(client_, timeout_ms);

    //tymczsow 1 proba
  registered_ = true;
  resp = true;
  
  yield();
  delay(1);
  client_.stop();
  return resp;
}


void SuplaEsphomeBridge::fill_channel_D(
    TDS_SuplaDeviceChannel_D &ch,
    uint8_t number,
    int type,
    int func,
    int default_value,
    int64_t flags,
    bool offline,
    unsigned value_validity_sec,
    const char *initial_value,
    uint8_t default_icon
) {
    // Wyzerowanie całej struktury (bardzo ważne!)
    memset(&ch, 0, sizeof(TDS_SuplaDeviceChannel_D));

    // Numer kanału
    ch.Number = number;

    // Typ kanału (np. SUPLA_CHANNELTYPE_RELAY)
    ch.Type = type;

    // Funkcja kanału (np. SUPLA_CHANNELFNC_POWERSWITCH)
    ch.FuncList = func;

    // Wartość domyślna (np. 0/1)
    ch.Default = default_value;

    // Flagi kanału (np. SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG)
    ch.Flags = flags;

    // Czy kanał jest offline
    ch.Offline = offline ? 1 : 0;

    // Czas ważności wartości (0 = bez limitu)
    ch.ValueValidityTimeSec = value_validity_sec;

    // Wartość początkowa
    if (initial_value) {
        strncpy(ch.value, initial_value, SUPLA_CHANNELVALUE_SIZE - 1);
        ch.value[SUPLA_CHANNELVALUE_SIZE - 1] = '\0';
    }

    // Ikona domyślna (np. SUPLA_ICON_RELAY)
    ch.DefaultIcon = default_icon;
}






bool SuplaEsphomeBridge::read_register_response(WiFiClient &client,
                                                unsigned long timeout_ms) {

  unsigned long start = millis();
  char inbuf[1536];

  while (millis() - start < timeout_ms) {
    yield();
    delay(1);
    if (client.available()) {

      int r = client.read(inbuf, sizeof(inbuf));
      if (r <= 0) continue;

          yield();
    delay(1);

      ESP_LOGI("supla", "Received %d bytes", r);
      hex_dump((uint8_t*)inbuf, r, "RX");

    yield();
    delay(1);


    }
  }

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

}  // namespace supla_esphome_bridge
