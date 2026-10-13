#pragma once

#include <Arduino.h>

#include "sensors.h"

// Comunicação com a nuvem (Nuxt na Vercel).
//  - Envio das médias de 10 min (POST /api/v1/ingest). As médias vão primeiro
//    para uma fila em LittleFS (/queue.jsonl): nada se perde se a internet ou a
//    nuvem estiverem fora; o envio é retomado depois.
//  - Cada envio leva uma cópia da configuração (display, calibração e
//    localização) para o painel mostrar. A configuração é feita só no ESP32:
//    quando ela muda, a cópia é enviada na hora, mesmo sem médias na fila.
// As chamadas HTTPS rodam numa tarefa própria (núcleo 0), sem travar o display.
// URL e token ficam na NVS (comandos "nuvem url" e "nuvem token").

struct UplinkStatus {
  bool configured = false;   // URL e token definidos
  uint32_t queue = 0;        // médias aguardando envio
  uint32_t lastOkEpoch = 0;  // último envio aceito (UTC)
  int lastHttp = 0;          // último status HTTP (ou código negativo de erro)
  String lastError;
  String url;
  uint32_t configSentEpoch = 0;   // última cópia da configuração aceita pela nuvem
  bool configPending = false;     // configuração mudou e a cópia ainda não chegou
  uint32_t reportEpoch = 0;       // último aviso "Está chovendo aqui" aceito pela nuvem
  bool reportPending = false;
};

// Chance de chuva nas próximas 1, 2 e 3 h, calculada na nuvem e recebida na resposta do envio.
struct RainForecast {
  bool valid = false;
  uint32_t at = 0;       // epoch UTC do cálculo
  uint8_t p[3] = {0};    // %
  char method[3][3] = {{0}};  // "M1", "M2", "M3"
  String reason;
  // Recente o bastante para mostrar (até 35 min; depois a estação some com ela).
  bool fresh() const { return valid && at && (uint32_t)time(nullptr) - at <= 35 * 60; }
};

void uplinkBegin();

// Chamar a cada leitura (5 s): acumula a média do intervalo de 10 min.
void uplinkAddReading(const Reading &r);

// Chamar no loop(): prepara a cópia da configuração quando ela muda.
void uplinkLoop();

// Cópia do estado (seguro para chamar de qualquer lugar).
UplinkStatus uplinkStatus();

bool uplinkSetUrl(const String &url, String &err);
bool uplinkSetToken(const String &token, String &err);
void uplinkDisable();        // apaga URL e token
void uplinkSendNow();        // força o envio da fila e da cópia da configuração
uint32_t uplinkClearQueue(); // descarta a fila; retorna quantas médias foram apagadas
RainForecast uplinkRain();   // última previsão de chuva recebida
void uplinkReportRain();     // repassa o aviso "Está chovendo aqui" para a nuvem
