#include "supla_esphome_bridge.h"

namespace supla_esphome_bridge {

// GUID: 1C81FE5A-DDDD-BCD1-FCC1-0F42C159618E
const uint8_t SuplaEsphomeBridge::GUID_BIN[16] = {
  0x1C, 0x81, 0xFE, 0x5A, 0xDD, 0xDD, 0xBC, 0xD1,
  0xFC, 0xC1, 0x0F, 0x42, 0xC1, 0x59, 0x61, 0x8E
};

SuplaEsphomeBridge::SuplaEsphomeBridge() {
  // konstruktor
}

SuplaEsphomeBridge::~SuplaEsphomeBridge() {
  // destruktor
}

void SuplaEsphomeBridge::setup() {
  // nic specjalnego tutaj; rejestracja może być wywołana w loop lub ręcznie
  ESP_LOGI("supla", "SuplaEsphomeBridge setup()");
}

void SuplaEsphomeBridge::loop() {
  // jeśli nie zarejestrowane, spróbuj co 10s
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

bool SuplaEsphomeBridge::send_register_packet(WiFiClient &client) {
  // Budujemy strukturę rejestracyjną wersji F (typowa nazwa w proto.h: TDS_SuplaRegisterDevice_F)
  // Używamy sizeof pól, aby nie polegać na makrach.
  TDS_SuplaRegisterDevice_F reg;
  memset(&reg, 0, sizeof(reg));

  // Wypełniamy pola, kopiując GUID, nazwę, softver, servername, location id i location pwd.
  // Używamy rozmiarów pól bezpośrednio z struktury.
  if (sizeof(reg.GUID) >= 16) {
    memcpy(reg.GUID, GUID_BIN, 16);
  } else {
    // bezpieczne kopiowanie jeśli pole mniejsze (mało prawdopodobne)
    memcpy(reg.GUID, GUID_BIN, sizeof(reg.GUID));
  }

  // MAC: jeśli struktura ma pole MAC, ustawiamy na 0 (brak)
  if (sizeof(reg.MAC) >= 1) {
    memset(reg.MAC, 0, sizeof(reg.MAC));
  }

  // LocationID (jeśli pole istnieje)
  // Niektóre wersje mają LocationID jako uint32_t lub int32_t
  // Używamy rzutowania, aby dopasować typ.
  reg.LocationID = (uint32_t)location_id_;

  // LocationPWD: kopiujemy null-terminated, zgodnie z rozmiarem pola
  if (sizeof(reg.LocationPWD) > 0 && !location_password_.empty()) {
    size_t maxcpy = sizeof(reg.LocationPWD) - 1;
    if (maxcpy > 0) {
      strncpy((char*)reg.LocationPWD, location_password_.c_str(), maxcpy);
      reg.LocationPWD[maxcpy] = 0;
    }
  }

  // Name, SoftVer, ServerName
  if (sizeof(reg.Name) > 0) {
    size_t maxcpy = sizeof(reg.Name) - 1;
    strncpy((char*)reg.Name, device_name_.c_str(), maxcpy);
    reg.Name[maxcpy] = 0;
  }
  if (sizeof(reg.SoftVer) > 0) {
    const char *sv = "esphome-supla-bridge-1.0";
    size_t maxcpy = sizeof(reg.SoftVer) - 1;
    strncpy((char*)reg.SoftVer, sv, maxcpy);
    reg.SoftVer[maxcpy] = 0;
  }
  if (sizeof(reg.ServerName) > 0) {
    size_t maxcpy = sizeof(reg.ServerName) - 1;
    strncpy((char*)reg.ServerName, server_.c_str(), maxcpy);
    reg.ServerName[maxcpy] = 0;
  }

  // Logowanie struktury przed wysłaniem (hex dump)
  ESP_LOGI("supla", "Sending raw register struct, size=%u", (unsigned)sizeof(reg));
  hex_dump((const uint8_t*)&reg, sizeof(reg), "TX");

  // Wysyłamy surową strukturę
  size_t sent = client.write((const uint8_t*)&reg, sizeof(reg));
  if (sent != sizeof(reg)) {
    ESP_LOGW("supla", "Sent bytes mismatch: sent=%u expected=%u", (unsigned)sent, (unsigned)sizeof(reg));
    return false;
  }

  ESP_LOGI("supla", "Register struct sent");
  return true;
}

bool SuplaEsphomeBridge::read_register_response(WiFiClient &client, unsigned long timeout_ms) {
  unsigned long start = millis();
  const size_t BUF_SZ = 1024;
  uint8_t inbuf[BUF_SZ];

  // Oczekujemy odpowiedzi; serwer SUPLA zwykle zwraca strukturę TSD_SuplaRegisterDeviceResult (lub B variant)
  while (millis() - start < timeout_ms) {
    if (client.available()) {
      size_t avail = client.available();
      if (avail > BUF_SZ) avail = BUF_SZ;
      int r = client.read((char*)inbuf, avail);
      if (r <= 0) {
        delay(10);
        continue;
      }

      ESP_LOGI("supla", "Received %d bytes from SUPLA", r);
      hex_dump(inbuf, r, "RX");

      // Jeśli bufor jest co najmniej tak duży jak struktura wyniku, spróbuj zmapować
      if (r >= (int)sizeof(TSD_SuplaRegisterDeviceResult)) {
        TSD_SuplaRegisterDeviceResult res;
        // kopiujemy tylko tyle ile potrzeba (bez przekroczenia)
        memcpy(&res, inbuf, sizeof(res));

        // Loguj pola, które zwykle istnieją: result_code i activity_timeout
        // Nazwy pól mogą się różnić w Twoim proto.h; jeśli tak, dopasuj.
#ifdef SUPLA_RESULT_TRUE
        const int RESULT_TRUE = SUPLA_RESULT_TRUE;
#else
        const int RESULT_TRUE = 1;
#endif

        ESP_LOGI("supla", "Register result: result_code=%d activity_timeout=%u",
                 res.result_code, (unsigned)res.activity_timeout);

        if (res.result_code == RESULT_TRUE) {
          registered_ = true;
          ESP_LOGI("supla", "Device registered successfully (activity_timeout=%u)", (unsigned)res.activity_timeout);
          return true;
        } else {
          registered_ = false;
          ESP_LOGW("supla", "Registration failed, code=%d", res.result_code);
          return false;
        }
      } else {
        // Jeśli otrzymaliśmy mniej niż rozmiar struktury, kontynuujemy zbieranie (prosty tryb)
        // W prostym podejściu nie buforujemy wieloczęściowych odpowiedzi; można rozbudować.
        ESP_LOGD("supla", "Received %d bytes, waiting for full result struct (%u bytes needed)",
                 r, (unsigned)sizeof(TSD_SuplaRegisterDeviceResult));
      }
    }  // if available

    delay(10);
  }  // while timeout

  ESP_LOGW("supla", "Timeout waiting for register response");
  return false;
}

}  // namespace supla_esphome_bridge
