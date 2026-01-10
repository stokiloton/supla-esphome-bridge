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

// --- Kanał E (proto >= 25) ---
#ifndef TDS_SuplaDeviceChannel_E_DEFINED
#define TDS_SuplaDeviceChannel_E_DEFINED
typedef struct {
  unsigned char Number;
  _supla_int_t Type;
  _supla_int_t FuncList;
  _supla_int_t Default;
  _supla_int64_t Flags;
  unsigned char Offline;
  unsigned _supla_int_t ValueValidityTimeSec;
  char value[SUPLA_CHANNELVALUE_SIZE];
  unsigned char DefaultIcon;
  unsigned char SubDeviceId;
} TDS_SuplaDeviceChannel_E;
#endif

// --- Rejestracja G z kanałami E ---
#ifndef TDS_SuplaRegisterDevice_G_DEFINED
#define TDS_SuplaRegisterDevice_G_DEFINED
typedef struct {
  _supla_int_t LocationID;
  char LocationPWD[SUPLA_LOCATION_PWD_MAXSIZE];
  char GUID[SUPLA_GUID_SIZE];
  char Name[SUPLA_DEVICE_NAME_MAXSIZE];
  char SoftVer[SUPLA_SOFTVER_MAXSIZE];
  unsigned char channel_count;
  TDS_SuplaDeviceChannel_E channels[SUPLA_CHANNELMAXCOUNT];
} TDS_SuplaRegisterDevice_G;
#endif

namespace supla_esphome_bridge {

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
  if (!registered_ && millis() - last_try > 20000) {
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

  ESP_LOGI("supla", "Connecting to SUPLA server %s:2015", server_.c_str());
  if (!client_.connect(server_.c_str(), 2015)) {
    ESP_LOGW("supla", "Cannot connect to SUPLA server");
    return false;
  }

  const unsigned call_id = SUPLA_DS_CALL_REGISTER_DEVICE_G;
  ESP_LOGI("supla", "Attempting register with call_id=%u", call_id);

  // --- Build TDS_SuplaRegisterDevice_G (kanały E) ---
  TDS_SuplaRegisterDevice_G reg;
  memset(&reg, 0, sizeof(reg));

  reg.LocationID = (_supla_int_t)location_id_;

  // LocationPWD (null-terminated)
  {
    size_t maxcpy = SUPLA_LOCATION_PWD_MAXSIZE - 1;
    if (!location_password_.empty() && maxcpy > 0) {
      strncpy(reg.LocationPWD, location_password_.c_str(), maxcpy);
      reg.LocationPWD[maxcpy] = '\0';
    } else {
      reg.LocationPWD[0] = '\0';
    }
  }

  memcpy(reg.GUID, GUID_BIN, SUPLA_GUID_SIZE);

  // Name
  if (!device_name_.empty()) {
    size_t nmax = SUPLA_DEVICE_NAME_MAXSIZE - 1;
    strncpy(reg.Name, device_name_.c_str(), nmax);
    reg.Name[nmax] = '\0';
  }

  // SoftVer
  {
    const char *softver = "esphome-supla-bridge-1.0";
    strncpy(reg.SoftVer, softver, SUPLA_SOFTVER_MAXSIZE - 1);
    reg.SoftVer[SUPLA_SOFTVER_MAXSIZE - 1] = '\0';
  }

  // --- One channel: TDS_SuplaDeviceChannel_E ---
  reg.channel_count = 1;
  TDS_SuplaDeviceChannel_E &ch = reg.channels[0];
  memset(&ch, 0, sizeof(ch));

  ch.Number = 0;
  ch.Type = SUPLA_CHANNELTYPE_THERMOMETER;

#ifdef SUPLA_BIT_FUNC_THERMOMETER
  ch.FuncList = (_supla_int_t)SUPLA_BIT_FUNC_THERMOMETER;
#else
  ch.FuncList = (_supla_int_t)0x00000100;
#endif

  ch.Default = 0;
  ch.Flags = 0;
  ch.Offline = 0;
  ch.ValueValidityTimeSec = 0;
  memset(ch.value, 0, SUPLA_CHANNELVALUE_SIZE);
  ch.DefaultIcon = 0;
  ch.SubDeviceId = 0;

  // --- Payload size ---
  size_t payload_size =
      offsetof(TDS_SuplaRegisterDevice_G, channels) +
      reg.channel_count * sizeof(TDS_SuplaDeviceChannel_E);

  ESP_LOGI("supla", "Prepared register payload_size=%u (channel_count=%u)",
           (unsigned)payload_size, (unsigned)reg.channel_count);
  hex_dump((const uint8_t*)&reg, payload_size, "REG-PAYLOAD");

  // --- SDP (RAW) ---
  TSuplaDataPacket *sdp = sproto_sdp_malloc(sproto_ctx_);
  if (!sdp) {
    ESP_LOGW("supla", "sproto_sdp_malloc failed");
    client_.stop();
    return false;
  }

  sproto_sdp_init(sproto_ctx_, sdp);

  if (!sproto_set_data(sdp, (char*)&reg, (unsigned _supla_int_t)payload_size, call_id)) {
    ESP_LOGW("supla", "sproto_set_data failed");
    sproto_sdp_free(sdp);
    client_.stop();
    return false;
  }

  size_t packet_len =
      sizeof(TSuplaDataPacket) - SUPLA_MAX_DATA_SIZE + sdp->data_size;

  ESP_LOGI("supla", "Sending raw TSuplaDataPacket (call_id=%u), len=%u",
           call_id, (unsigned)packet_len);
  hex_dump((uint8_t*)sdp, packet_len, "TX");

  size_t sent = client_.write((uint8_t*)sdp, packet_len);
  client_.flush();
  sproto_sdp_free(sdp);

  if (sent != packet_len) {
    ESP_LOGW("supla", "Sent mismatch: %u != %u",
             (unsigned)sent, (unsigned)packet_len);
    client_.stop();
    return false;
  }

  ESP_LOGI("supla", "Register SDP sent");

  bool resp = read_register_response(client_, timeout_ms);
  client_.stop();
  return resp;
}

bool SuplaEsphomeBridge::read_register_response(WiFiClient &client,
                                                unsigned long timeout_ms) {

  unsigned long start = millis();
  char inbuf[1536];

  while (millis() - start < timeout_ms) {

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
              return true;
            }

            registered_ = false;
            return false;
          }
        }
      }
    }

    delay(10);
  }

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

} // namespace supla_esphome_bridge
