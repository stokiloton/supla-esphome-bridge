#include "supla_esphome_bridge.h"
#include <cstddef>   // offsetof
#include <cstring>   // memset, strncpy
#include <cstdint>

namespace supla_esphome_bridge {

// GUID: 1C81FE5A-DDDD-BCD1-FCC1-0F42C159618E
const uint8_t SuplaEsphomeBridge::GUID_BIN[SUPLA_GUID_SIZE] = {
  0x1C, 0x81, 0xFE, 0x5A, 0xDD, 0xDD, 0xBC, 0xD1,
  0xFC, 0xC1, 0x0F, 0x42, 0xC1, 0x59, 0x61, 0x8E
};

SuplaEsphomeBridge::SuplaEsphomeBridge() {
  sproto_ctx_ = sproto_init();
  if (sproto_ctx_) {
#ifdef SUPLA_PROTO_VERSION
    sproto_set_version(sproto_ctx_, SUPLA_PROTO_VERSION);
    ESP_LOGI("supla", "sproto initialized, forced proto version=%u", (unsigned)SUPLA_PROTO_VERSION);
#else
    ESP_LOGI("supla", "sproto initialized");
#endif
  } else {
    ESP_LOGW("supla", "sproto_init failed or not available");
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
    int pos = snprintf(line, sizeof(line), "%s %04X: ", prefix, (unsigned int)i);
    for (size_t j = 0; j < chunk && pos < (int)sizeof(line) - 4; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i + j]);
    }
    pos += snprintf(line + pos, sizeof(line) - pos, " | ");
    for (size_t j = 0; j < chunk && pos < (int)sizeof(line) - 2; j++) {
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
    ESP_LOGW("supla", "Cannot connect to SUPLA server %s", server_.c_str());
    return false;
  }

#ifdef SUPLA_DS_CALL_REGISTER_DEVICE_G
  const unsigned call_id = SUPLA_DS_CALL_REGISTER_DEVICE_G;
#else
  const unsigned call_id = 76;
#endif

  ESP_LOGI("supla", "Attempting register with call_id=%u (G)", call_id);

  // Prepare registration structure
  TDS_SuplaRegisterDevice reg;
  memset(&reg, 0, sizeof(reg));

  reg.LocationID = (_supla_int_t)location_id_;
  if (!location_password_.empty()) {
    size_t maxcpy = (SUPLA_LOCATION_PWD_MAXSIZE > 0) ? SUPLA_LOCATION_PWD_MAXSIZE - 1 : 0;
    if (maxcpy) {
      strncpy(reg.LocationPWD, location_password_.c_str(), maxcpy);
      reg.LocationPWD[maxcpy] = 0;
    }
  }

  memcpy(reg.GUID, GUID_BIN, SUPLA_GUID_SIZE);

  if (!device_name_.empty()) {
    size_t maxcpy = (SUPLA_DEVICE_NAME_MAXSIZE > 0) ? SUPLA_DEVICE_NAME_MAXSIZE - 1 : 0;
    if (maxcpy) {
      strncpy(reg.Name, device_name_.c_str(), maxcpy);
      reg.Name[maxcpy] = 0;
    }
  }

  const char *softver = "esphome-supla-bridge-1.0";
  if (SUPLA_SOFTVER_MAXSIZE > 0) {
    strncpy(reg.SoftVer, softver, SUPLA_SOFTVER_MAXSIZE - 1);
    reg.SoftVer[SUPLA_SOFTVER_MAXSIZE - 1] = 0;
  }

  // --- Add one minimal channel (TDS_SuplaDeviceChannel_E layout) ---
  reg.channel_count = 1;
  memset(&reg.channels[0], 0, sizeof(reg.channels[0]));

  // Fields present in TDS_SuplaDeviceChannel_E
  reg.channels[0].Number = 0;
  reg.channels[0].Type = SUPLA_CHANNELTYPE_THERMOMETER;

#ifdef SUPLA_BIT_FUNC_THERMOMETER
  reg.channels[0].FuncList = (_supla_int_t)SUPLA_BIT_FUNC_THERMOMETER;
#else
  reg.channels[0].FuncList = (_supla_int_t)0x00000100;
#endif

  reg.channels[0].Default = 0;
  reg.channels[0].Flags = 0;
  reg.channels[0].Offline = 0;
  reg.channels[0].ValueValidityTimeSec = 0;
  memset(reg.channels[0].value, 0, SUPLA_CHANNELVALUE_SIZE);
  reg.channels[0].DefaultIcon = 0;
  reg.channels[0].SubDeviceId = 0;

  // Compute actual payload size: header up to channels + channel_count * sizeof(channel)
  size_t payload_size = offsetof(TDS_SuplaRegisterDevice, channels);
  unsigned cc = (unsigned)reg.channel_count;
  if (cc > SUPLA_CHANNELMAXCOUNT) cc = SUPLA_CHANNELMAXCOUNT;
  payload_size += cc * sizeof(TDS_SuplaDeviceChannel);
  unsigned _supla_int_t datasz = (unsigned _supla_int_t)payload_size;

  ESP_LOGI("supla", "Prepared register payload_size=%u (channel_count=%u) for call_id=%u",
           (unsigned)payload_size, (unsigned)reg.channel_count, call_id);
  hex_dump((const uint8_t*)&reg, payload_size, "REG-PAYLOAD");

  // Allocate SDP
  TSuplaDataPacket *sdp = sproto_sdp_malloc(sproto_ctx_);
  if (!sdp) {
    ESP_LOGW("supla", "sproto_sdp_malloc returned NULL");
    client_.stop();
    return false;
  }
  sproto_sdp_init(sproto_ctx_, sdp);

  // Try to set data via helper
  bool set_ok = sproto_set_data(sdp, (char*)&reg, datasz, call_id);
  if (!set_ok) {
    ESP_LOGW("supla", "sproto_set_data returned false for call_id=%u. Trying fallback...", call_id);
#ifdef SUPLA_MAX_DATA_SIZE
    const unsigned max_data = SUPLA_MAX_DATA_SIZE;
#else
    const unsigned max_data = 2048;
#endif
    if (sdp->data && max_data >= datasz) {
      memcpy(sdp->data, &reg, datasz);
      sdp->data_size = datasz;
      sdp->call_id = call_id;
      ESP_LOGI("supla", "Fallback: copied payload into sdp->data (data_size=%u) call_id=%u",
               (unsigned)sdp->data_size, call_id);
    } else {
      ESP_LOGW("supla", "Fallback failed: sdp->data NULL or max_data (%u) < datasz (%u)", max_data, (unsigned)datasz);
      sproto_sdp_free(sdp);
      client_.stop();
      return false;
    }
  }

#ifndef SPROTO_WITHOUT_OUT_BUFFER
  const size_t OUTBUF_SZ = 4096;
  char outbuf[OUTBUF_SZ];
  unsigned _supla_int_t outlen = sproto_pop_out_data(sproto_ctx_, outbuf, OUTBUF_SZ);
  if (outlen == 0) {
    ESP_LOGW("supla", "sproto_pop_out_data returned 0 for call_id=%u", call_id);
    sproto_sdp_free(sdp);
    client_.stop();
    return false;
  }
  client_.setNoDelay(true);
  ESP_LOGI("supla", "Sending register SDP (call_id=%u), bytes=%u", call_id, (unsigned)outlen);
  hex_dump((const uint8_t*)outbuf, outlen, "TX");
  size_t sent = client_.write((const uint8_t*)outbuf, outlen);
  client_.flush();
  sproto_sdp_free(sdp);
  if (sent != outlen) {
    ESP_LOGW("supla", "Sent bytes mismatch: sent=%u expected=%u", (unsigned)sent, (unsigned)outlen);
    client_.stop();
    return false;
  }
#else
  unsigned _supla_int_t data_size = sdp->data_size;
  size_t packet_len = sizeof(TSuplaDataPacket) - SUPLA_MAX_DATA_SIZE + data_size;
  client_.setNoDelay(true);
  ESP_LOGI("supla", "Sending raw TSuplaDataPacket (call_id=%u), len=%u", call_id, (unsigned)packet_len);
  hex_dump((const uint8_t*)sdp, packet_len, "TX");
  size_t sent = client_.write((const uint8_t*)sdp, packet_len);
  client_.flush();
  sproto_sdp_free(sdp);
  if (sent != packet_len) {
    ESP_LOGW("supla", "Sent bytes mismatch: sent=%u expected=%u", (unsigned)sent, (unsigned)packet_len);
    client_.stop();
    return false;
  }
#endif

  ESP_LOGI("supla", "Register SDP sent (call_id=%u)", call_id);

  // Wait for response
  bool resp = read_register_response(client_, timeout_ms);
  client_.stop();
  return resp;
}

bool SuplaEsphomeBridge::read_register_response(WiFiClient &client, unsigned long timeout_ms) {
  if (!sproto_ctx_) {
    ESP_LOGW("supla", "sproto context not initialized");
    return false;
  }

  unsigned long start = millis();
  const size_t INBUF_SZ = 1536;
  char inbuf[INBUF_SZ];

  while (millis() - start < timeout_ms) {
    if (client.available()) {
      int toread = client.available();
      if (toread > (int)INBUF_SZ) toread = INBUF_SZ;
      int r = client.read(inbuf, toread);
      if (r <= 0) {
        delay(10);
        continue;
      }

      ESP_LOGI("supla", "Received %d bytes from SUPLA", r);
      hex_dump((const uint8_t*)inbuf, (size_t)r, "RX");

      sproto_in_buffer_append(sproto_ctx_, inbuf, (unsigned _supla_int_t)r);

      TSuplaDataPacket sdp;
      while (sproto_pop_in_sdp(sproto_ctx_, &sdp)) {
        ESP_LOGI("supla", "Parsed SDP: call_id=%u data_size=%u", (unsigned)sdp.call_id, (unsigned)sdp.data_size);
        if (sdp.data && sdp.data_size > 0) {
          hex_dump((const uint8_t*)sdp.data, (size_t)sdp.data_size, "SDP_DATA");
        }

        if (sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT || sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B) {
          if ((unsigned)sdp.data_size >= sizeof(TSD_SuplaRegisterDeviceResult)) {
            TSD_SuplaRegisterDeviceResult res;
            memset(&res, 0, sizeof(res));
            size_t copy = (sdp.data_size >= sizeof(res)) ? sizeof(res) : sdp.data_size;
            memcpy(&res, sdp.data, copy);

            ESP_LOGI("supla", "Register result: result_code=%d activity_timeout=%u version=%u version_min=%u",
                     (int)res.result_code, (unsigned)res.activity_timeout, (unsigned)res.version, (unsigned)res.version_min);

            if (res.result_code == SUPLA_RESULT_TRUE) {
              registered_ = true;
              ESP_LOGI("supla", "Device registered successfully (activity_timeout=%u)", (unsigned)res.activity_timeout);
              return true;
            } else {
              registered_ = false;
              ESP_LOGW("supla", "Registration failed, code=%d", (int)res.result_code);
              return false;
            }
          } else {
            ESP_LOGW("supla", "Register result SDP too small: %u", (unsigned)sdp.data_size);
          }
        } else {
          ESP_LOGD("supla", "Unhandled call_id %u", (unsigned)sdp.call_id);
        }
      }  // while pop sdp
    } else {
      delay(10);
    }
  }  // while timeout

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

}  // namespace supla_esphome_bridge
