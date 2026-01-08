# supla-esphome-bridge

Most pomiędzy ESPHome a chmurą SUPLA, implementowany jako `external_component`.

Funkcje:
- rejestracja urządzenia w SUPLA przy użyciu `Identyfikator Lokalizacji + Hasło Lokalizacji`,
- kanał temperatury (odczyt z ESPHome),
- kanał przekaźnika (sterowanie z ESPHome i z chmury SUPLA),
- prosty protokół binarny (minimalny wycinek pod termometr + przekaźnik).

Użycie w ESPHome:

```yaml
external_components:
  - source: github://twoj-user/supla-esphome-bridge
    components: [supla_esphome_bridge]

supla_esphome_bridge:
  server: "svr1.supla.org"
  location_id: 12345
  location_password: "00112233445566778899AABBCCDDEEFF"
  device_name: "esphome"
  temperature: termometr1_temp
  switch: termometr1_switch
