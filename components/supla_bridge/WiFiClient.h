#ifndef PROJECT_WIFICLIENT_H_
#define PROJECT_WIFICLIENT_H_

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
// Provide a minimal stub to avoid build failure on non-ESP platforms.
// This will not provide real WiFi functionality.
class WiFiClient {
 public:
  WiFiClient() {}
  virtual ~WiFiClient() {}
  virtual int connect(const char *host, uint16_t port) { (void)host; (void)port; return 0; }
  virtual int available() { return 0; }
  virtual int read(uint8_t *buf, size_t size) { (void)buf; (void)size; return -1; }
  virtual size_t write(const uint8_t *buf, size_t size) { (void)buf; (void)size; return 0; }
  virtual void stop() {}
  virtual uint32_t localIP() const { return 0; }
  virtual bool connected() const { return false; }
};

class WiFiClientSecure : public WiFiClient { };

#endif

#endif  // PROJECT_WIFICLIENT_H_
