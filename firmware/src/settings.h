#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Páginas do display e campos da página "Resumo". Os nomes em JSON são os
// mesmos que o painel usa para mostrar a cópia (web/shared/utils/station-config.ts).
enum PageType : uint8_t { PG_TEMP, PG_HUM, PG_PRES, PG_CLOCK, PG_TODAY, PG_FIELDS, PG_DETAILS, PG_NETWORK, PG_RAIN, PG_COUNT };
enum FieldType : uint8_t { FD_TEMP, FD_HUM, FD_PRES, FD_DEW, FD_HEAT, FD_TEMP_BMP, FD_PRES_STATION, FD_COUNT };

static const int MAX_PAGES = 8;
static const int MAX_FIELDS = 4;

struct PageCfg {
  uint8_t type = PG_TEMP;
  uint8_t nfields = 0;
  uint8_t fields[MAX_FIELDS] = {0};
};

// Configuração persistente em LittleFS (/config.json).
// Os valores iniciais vêm de include/config.h e include/site.h; depois de
// gravada, a configuração do arquivo prevalece. Tudo é editado aqui mesmo
// (página local, API e console); a nuvem só recebe uma cópia.
// Segredos não ficam aqui (ficam na NVS).
struct Settings {
  int schema = 4;
  String deviceId;
  String hostname;     // nome mDNS: <hostname>.local
  String tz;           // fuso no formato POSIX, ex.: "<-03>3"
  String ntp1, ntp2;
  float groundAltM = 0;
  int floor = 0;
  float floorHeightM = 2.9f;
  float sensorHeightM = 1.0f;
  // display
  uint32_t displayPageMs = 5000;
  uint8_t displayContrast = 128;
  PageCfg pages[MAX_PAGES];
  uint8_t pageCount = 0;
  bool nightEnabled = false;
  uint16_t nightFrom = 23 * 60;  // minutos desde 00:00
  uint16_t nightTo = 6 * 60;
  uint8_t nightMode = 1;         // 0 = apagar, 1 = escurecer
  uint8_t pageMode = 1;          // troca de página: 0 = automática, 1 = pelo botão, 2 = as duas
  uint8_t idleDimMin = 10;       // modo pelo botão: escurece após N min sem toque (0 = nunca)
  // buzzer e LED (Fase 19)
  bool buzzerEnabled = true;
  bool buzzerClicks = true;      // clique curto a cada toque no botão
  bool buzzerActiveLow = false;  // o módulo liga com nível baixo
  bool quietEnabled = true;      // horário silencioso: buzzer calado e LED em led.quiet
  uint16_t quietFrom = 22 * 60;
  uint16_t quietTo = 7 * 60;
  uint8_t ledMode = 0;           // 0 = normal, 1 = só avisos, 2 = desligado
  uint8_t ledQuietMode = 1;      // o mesmo, no horário silencioso
  // calibração (somada às leituras)
  float calTemp = 0, calHum = 0, calPres = 0;
  // pressão calibrada pelo QNH do aeroporto: o desvio vem da nuvem e substitui calPres
  bool presAuto = true;
  float presAutoOffset = 0;  // hPa, somado à pressão ao nível do mar
  // alerta de chuva no display (a previsão vem da nuvem)
  bool rainAlert = true;
  uint8_t rainAlertPct = 60;    // dispara a partir desta chance
  uint8_t rainAlertHours = 2;   // olhando as próximas 1, 2 ou 3 h
  uint8_t rainAlertMode = 3;    // bit 0 = ícone no cabeçalho, bit 1 = tela de alerta no carrossel
  // localização para os dados externos (NAN = não definida)
  float lat = NAN, lon = NAN;
  String placeLabel;   // nome curto do local, ex.: "Casa"
};

const char *pageTypeName(uint8_t t);
const char *fieldTypeName(uint8_t f);

// Monta o LittleFS e carrega (ou cria) /config.json.
bool settingsBegin();

// Configuração atual (somente leitura).
const Settings &settings();

// Altitude do sensor calculada a partir da configuração.
float settingsAltitudeM();

// Altera um campo pelo nome (ex.: "site.floor", "display.page_s") e salva.
// Retorna false e preenche err se a chave ou o valor forem inválidos.
bool settingsSet(const String &key, const String &value, String &err);

// Aplica várias alterações de uma vez a partir de um JSON parcial no mesmo
// formato de /config.json. Tudo ou nada: se um campo for inválido, nada muda.
bool settingsApplyJson(JsonObjectConst obj, String &err);

// Muda a cada gravação: quem precisa reagir a mudanças compara com o valor anterior.
uint32_t settingsRevision();

// Cópia enviada para a nuvem: display, calibração, localização, alerta, buzzer, LED e horário silencioso.
void settingsCloudCopy(JsonDocument &doc);

// Serializa a configuração atual (formato de /config.json).
void settingsToJson(JsonDocument &doc);

// Volta aos valores iniciais (config.h/site.h) e salva. Mantém o device_id.
bool settingsReset();

// Imprime a configuração no serial, em JSON.
void settingsPrint();
