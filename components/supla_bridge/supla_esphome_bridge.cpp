#include "supla_esphome_bridge.h"
#include <cstddef>
#include <cstring>
#include <cstdint>

#ifndef SUPLA_PROTO_VERSION
#define SUPLA_PROTO_VERSION 28
#endif

#ifndef SUPLA_DS_CALL_REGISTER_DEVICE_G
#define SUPLA_DS_CALL_REGISTER_DEVICE_G 76
#endif

namespace supla_esphome_bridge {

// GUID: 1C81FE5A-DDDD-BCD1-FCC1-0F42C159618E
const uint8_t SuplaEsphomeBridge::GUID_BIN[SUPLA_GUID_SIZE] = {
  0x1C, 0x81, 0xFE, 0x5A, 0xDD, 0xDD, 0xBC, 0xD1,
  0xFC, 0xC1, 0x0F, 0x42, 0xC1, 0x59, 0x61, 0x8E
};

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
  if (!registered_ && millis() - last_try > 30000) {
    last_try = millis();
    register_device(10000);
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
      ok = true; break;
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
  
  const unsigned call_id = SUPLA_DS_CALL_REGISTER_DEVICE_G;
  ESP_LOGI("supla", "Attempting register with call_id=%u (REGISTER_DEVICE_G)", call_id);
  // -------------------------
  // Build TDS_SuplaRegisterDevice_G (Email + AuthKey)
  // -------------------------
  TDS_SuplaRegisterDevice_G reg;
  memset(&reg, 0, sizeof(reg));

  // EMAIL
  strncpy(reg.Email, "tmp.spam.stokiloton@gmail.com", SUPLA_EMAIL_MAXSIZE - 1);

  // AUTHKEY (16 bytes, stały)
  static const uint8_t AUTHKEY[SUPLA_AUTHKEY_SIZE] = {
    0xA3,0x5F,0x91,0x0C,0x77,0x2B,0x4E,0x88,
    0x19,0xC4,0x5A,0x0D,0x6E,0x3F,0x12,0x99
  };
  memcpy(reg.AuthKey, AUTHKEY, SUPLA_AUTHKEY_SIZE);

  // GUID
  memcpy(reg.GUID, GUID_BIN, SUPLA_GUID_SIZE);


  strncpy(reg.Name, device_name_.c_str(), SUPLA_DEVICE_NAME_MAXSIZE - 1);


 // SoftVer
  strncpy(reg.SoftVer, device_name_.c_str(), SUPLA_SOFTVER_MAXSIZE - 1);

   // ServerName (musi odpowiadać temu, co wpisujesz jako server)
  strncpy(reg.ServerName, server_.c_str(), SUPLA_SERVER_NAME_MAXSIZE - 1);
  
  // Flags, ManufacturerID, ProductID
  reg.Flags = 0;
  reg.ManufacturerID = 0;
  reg.ProductID = 0;

  yield();
  delay(1);

  // -------------------------
  // One channel (TDS_SuplaDeviceChannel_E)
  // -------------------------
  reg.channel_count = 1;

  TDS_SuplaDeviceChannel_E &ch = reg.channels[0];
  memset(&ch, 0, sizeof(ch));

  ch.Number = 0;
  ch.Type = SUPLA_CHANNELTYPE_THERMOMETER;
  ch.FuncList = SUPLA_BIT_FUNC_THERMOMETER;
  ch.Default = 0;
  ch.Flags = 0;
  ch.Offline = 0;
  ch.ValueValidityTimeSec = 0;
  memset(ch.value, 0, SUPLA_CHANNELVALUE_SIZE);
  ch.DefaultIcon = 0;
  ch.SubDeviceId = 0;

  // -------------------------
  // Payload size
  // -------------------------
  size_t payload_size =
      offsetof(TDS_SuplaRegisterDevice_G, channels) +
      reg.channel_count * sizeof(TDS_SuplaDeviceChannel_E);

  ESP_LOGI("supla", "Prepared REGISTER_DEVICE_G payload_size=%u (channel_count=%u)",
           (unsigned)payload_size, (unsigned)reg.channel_count);

  yield();
  delay(1);
  
  //hex_dump((const uint8_t*)&reg, payload_size, "REG-PAYLOAD");

 // -------------------------
  // Build SDP (RAW MODE)
  // -------------------------
  TSuplaDataPacket *sdp = sproto_sdp_malloc(sproto_ctx_);
  if (!sdp) {
    ESP_LOGW("supla", "sproto_sdp_malloc failed");
    client_.stop();
    return false;
  }
  ESP_LOGW("supla", "sproto_sdp_malloc OK");
  yield();
  delay(1);


  

  bool resp = read_register_response(client_, timeout_ms);
  client_.stop();
  return resp;
}

bool SuplaEsphomeBridge::read_register_response(WiFiClient &client,
                                                unsigned long timeout_ms) {

  unsigned long start = millis();
  char inbuf[1536];

  while (millis() - start < timeout_ms) {
    delay(1);
    yield();
  

    if (client.available()) {

      int r = client.read(inbuf, sizeof(inbuf));
      if (r <= 0) continue;

      ESP_LOGI("supla", "Received %d bytes", r);
      hex_dump((uint8_t*)inbuf, r, "RX");

      sproto_in_buffer_append(sproto_ctx_, inbuf, r);

      TSuplaDataPacket sdp;
      while (sproto_pop_in_sdp(sproto_ctx_, &sdp)) {

        ESP_LOGI("supla", "Parsed SDP: call_id=%u size=%u",
                 (unsigned)sdp.call_id, (unsigned)sdp.data_size);

        if (sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT ||
            sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B) {

          if (sdp.data_size >= sizeof(TSD_SuplaRegisterDeviceResult)) {

            TSD_SuplaRegisterDeviceResult res;
            memcpy(&res, sdp.data, sizeof(res));

            ESP_LOGI("supla",
                     "Register result: code=%d timeout=%u version=%u min=%u",
                     res.result_code, res.activity_timeout,
                     res.version, res.version_min);

            if (res.result_code == SUPLA_RESULT_TRUE) {
              registered_ = true;
              ESP_LOGI("supla", "Device registered successfully");
              return true;
            }

            registered_ = false;
            ESP_LOGW("supla", "Registration failed, code=%d", res.result_code);
            return false;
          } else {
            ESP_LOGW("supla", "Register result SDP too small: %u",
                     (unsigned)sdp.data_size);
          }
        }
      }
    }

    delay(10);
  }

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

}  // namespace supla_esphome_bridge
