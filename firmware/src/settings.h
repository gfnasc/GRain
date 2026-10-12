#pragma once

#include <Arduino.h>

// Configuração persistente em LittleFS (/config.json).
// Os valores iniciais vêm de include/config.h e include/site.h; depois de
// gravada, a configuração do arquivo prevalece. Segredos não ficam aqui
// (senha do Wi-Fi fica na NVS, gerenciada pelo próprio ESP32).
struct Settings {
  int schema = 1;
  String deviceId;
  String hostname;     // nome mDNS: <hostname>.local
  String tz;           // fuso no formato POSIX, ex.: "<-03>3"
  String ntp1, ntp2;
  float groundAltM = 0;
  int floor = 0;
  float floorHeightM = 2.9f;
  float sensorHeightM = 1.0f;
  uint32_t displayPageMs = 5000;
  uint8_t displayContrast = 128;
};

// Monta o LittleFS e carrega (ou cria) /config.json.
bool settingsBegin();

// Configuração atual (somente leitura).
const Settings &settings();

// Altitude do sensor calculada a partir da configuração.
float settingsAltitudeM();

// Altera um campo pelo nome (ex.: "site.floor", "display.page_s") e salva.
// Retorna false e preenche err se a chave ou o valor forem inválidos.
bool settingsSet(const String &key, const String &value, String &err);

// Volta aos valores iniciais (config.h/site.h) e salva.
bool settingsReset();

// Imprime a configuração no serial, em JSON.
void settingsPrint();
