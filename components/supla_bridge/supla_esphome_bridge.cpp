#include "supla_esphome_bridge.h"

namespace supla_esphome_bridge {

// GUID: 1C81FE5A-DDDD-BCD1-FCC1-0F42C159618E
const uint8_t SuplaEsphomeBridge::GUID_BIN[16] = {
  0x1C, 0x81, 0xFE, 0x5A, 0xDD, 0xDD, 0xBC, 0xD1,
  0xFC, 0xC1, 0x0F, 0x42, 0xC1, 0x59, 0x61, 0x8E
};

SuplaEsphomeBridge::SuplaEsphomeBridge() {
  // inicjalizacja sproto/srpc jeśli dostępne w projekcie
  sproto_ctx_ = sproto_init();
  if (sproto_ctx_) {
    sproto_set_version(sproto_ctx_, SUPLA_PROTO_VERSION);
    ESP_LOGI("supla", "sproto initialized, proto version %d", SUPLA_PROTO_VERSION);
  } else {
    ESP_LOGW("supla", "sproto init failed or not available");
  }
}

SuplaEsphomeBridge::~SuplaEsphomeBridge() {
  if (sproto_ctx_) {
    sproto_free(sproto_ctx_);
    sproto_ctx_ = nullptr;
  }
}

void SuplaEsphomeBridge::setup() {
  // nic szczególnego tutaj; rejestracja może być wywołana w loop lub ręcznie
}

void SuplaEsphomeBridge::loop() {
  static unsigned long last_try = 0;
  if (!registered_ && millis() - last_try > 10000) {
    last_try = millis();
    register_device(10000);
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

  bool ok = send_register_packet(client_);
  if (!ok) {
    ESP_LOGW("supla", "Failed to send register packet");
    client_.stop();
    return false;
  }

  ok = read_register_response(client_, timeout_ms);
  client_.stop();
  return ok;
}

void SuplaEsphomeBridge::hex_dump(const uint8_t *buf, size_t len, const char *prefix) {
  // Loguj wierszami po 16 bajtów
  const size_t per_line = 16;
  char line[128];
  for (size_t i = 0; i < len; i += per_line) {
    size_t chunk = (i + per_line <= len) ? per_line : (len - i);
    int pos = snprintf(line, sizeof(line), "%s %04X: ", prefix, (unsigned int)i);
    for (size_t j = 0; j < chunk && pos < (int)sizeof(line) - 3; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i + j]);
    }
    // dopisz ASCII (czytelność)
    pos += snprintf(line + pos, sizeof(line) - pos, " | ");
    for (size_t j = 0; j < chunk && pos < (int)sizeof(line) - 2; j++) {
      char c = buf[i + j];
      line[pos++] = (c >= 32 && c < 127) ? c : '.';
    }
    line[pos] = 0;
    ESP_LOGD("supla", "%s", line);
  }
}

bool SuplaEsphomeBridge::send_register_packet(WiFiClient &client) {
  if (!sproto_ctx_) {
    ESP_LOGW("supla", "sproto not initialized - cannot build SDP");
    return false;
  }

  // Przygotuj strukturę rejestracyjną (wersja F - minimalna)
  TSD_SuplaRegisterDevice_F reg_f;
  memset(&reg_f, 0, sizeof(reg_f));

  // Wypełnij pola zgodnie z proto.h
  memcpy(reg_f.GUID, GUID_BIN, SUPLA_GUID_SIZE);
  // MAC nie mamy - ustawiamy na 0
  memset(reg_f.MAC, 0, SUPLA_MAC_SIZE);
  reg_f.LocationID = (uint32_t)location_id_;
  if (!location_password_.empty()) {
    strncpy((char*)reg_f.LocationPWD, location_password_.c_str(), sizeof(reg_f.LocationPWD) - 1);
  }
  strncpy((char*)reg_f.Name, device_name_.c_str(), sizeof(reg_f.Name) - 1);
  strncpy((char*)reg_f.SoftVer, "esphome-supla-bridge-1.0", sizeof(reg_f.SoftVer) - 1);
  strncpy((char*)reg_f.ServerName, server_.c_str(), sizeof(reg_f.ServerName) - 1);

  // Zbuduj SDP (Supla Data Packet) przy użyciu srpc/sproto helperów
  TSuplaDataPacket *sdp = sproto_sdp_malloc(sproto_ctx_);
  if (!sdp) {
    ESP_LOGW("supla", "sproto_sdp_malloc failed");
    return false;
  }
  sproto_sdp_init(sproto_ctx_, sdp);

  // ustaw call_id na rejestrację wersji F
  sproto_set_data(sdp, (char*)&reg_f, sizeof(reg_f), SUPLA_DS_CALL_REGISTER_DEVICE_F);

  // Wyciągnij dane z out buffer (jeśli implementacja sproto w repo ma out buffer)
  char outbuf[4096];
  unsigned _supla_int_t outlen = sproto_pop_out_data(sproto_ctx_, outbuf, sizeof(outbuf));
  if (outlen == 0) {
    // fallback: spróbuj serializacji bez out buffer (jeśli sdp ma bezpośrednie pole)
    if (sdp->data && sdp->data_size > 0) {
      outlen = sdp->data_size;
      if (outlen > (unsigned)_SUPLA_INT_MAX) {
        ESP_LOGW("supla", "Out data too large");
        sproto_sdp_free(sdp);
        return false;
      }
      memcpy(outbuf, sdp->data, outlen);
    } else {
      ESP_LOGW("supla", "No out data to send");
      sproto_sdp_free(sdp);
      return false;
    }
  }

  // Logowanie wysyłanych danych
  ESP_LOGI("supla", "Sending register packet, bytes=%u, call_id=%d", (unsigned)outlen, sdp->call_id);
  hex_dump((const uint8_t*)outbuf, outlen, "TX");

  // Wyślij
  size_t sent = client.write((const uint8_t*)outbuf, outlen);
  sproto_sdp_free(sdp);

  if (sent != outlen) {
    ESP_LOGW("supla", "Sent bytes mismatch: sent=%u expected=%u", (unsigned)sent, (unsigned)outlen);
    return false;
  }

  ESP_LOGI("supla", "Register packet sent successfully");
  return true;
}

bool SuplaEsphomeBridge::read_register_response(WiFiClient &client, unsigned long timeout_ms) {
  unsigned long start = millis();
  // Bufor wejściowy do logowania
  const size_t BUF_SZ = 2048;
  uint8_t inbuf[BUF_SZ];

  while (millis() - start < timeout_ms) {
    if (client.available()) {
      size_t avail = client.available();
      if (avail > BUF_SZ) avail = BUF_SZ;
      int r = client.read((char*)inbuf, avail);
      if (r <= 0) {
        delay(10);
        continue;
      }

      // Loguj surowe dane przychodzące
      ESP_LOGI("supla", "Received %d bytes from SUPLA", r);
      hex_dump(inbuf, r, "RX");

      // Dodaj dane do sproto in buffer i parsuj SDP
      sproto_in_buffer_append(sproto_ctx_, (char*)inbuf, (unsigned _supla_int_t)r);

      TSuplaDataPacket sdp;
      while (sproto_pop_in_sdp(sproto_ctx_, &sdp)) {
        ESP_LOGI("supla", "Parsed SDP: call_id=%d data_size=%d", sdp.call_id, sdp.data_size);
        // Loguj zawartość pola data (jeśli istnieje)
        if (sdp.data && sdp.data_size > 0) {
          hex_dump((const uint8_t*)sdp.data, sdp.data_size, "SDP_DATA");
        }

        // Obsługa wyników rejestracji (różne wersje)
        if (sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT ||
            sdp.call_id == SUPLA_SD_CALL_REGISTER_DEVICE_RESULT_B) {
          if (sdp.data_size >= (int)sizeof(TSD_SuplaRegisterDeviceResult)) {
            TSD_SuplaRegisterDeviceResult res;
            memcpy(&res, sdp.data, sizeof(res));
            ESP_LOGI("supla", "Register result: result_code=%d device_id=%u activity_timeout=%u",
                     res.result_code, res.DeviceID, res.activity_timeout);
            if (res.result_code == SUPLA_RESULT_TRUE) {
              registered_ = true;
              ESP_LOGI("supla", "Device registered successfully, device id=%u", res.DeviceID);
              return true;
            } else {
              registered_ = false;
              ESP_LOGW("supla", "Registration failed, code=%d", res.result_code);
              return false;
            }
          } else {
            ESP_LOGW("supla", "Register result packet too small: %d", sdp.data_size);
            // kontynuuj parsowanie dalszych SDP
          }
        } else {
          ESP_LOGD("supla", "Unhandled call_id %d", sdp.call_id);
        }
      }  // while pop sdp
    }    // if available

    delay(10);
  }  // while timeout

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

}  // namespace supla_esphome_bridge
