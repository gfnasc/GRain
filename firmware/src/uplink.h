#pragma once

#include <Arduino.h>

#include "sensors.h"

// Comunicação com a nuvem (Nuxt na Vercel).
//  - Envio das médias de 10 min (POST /api/v1/ingest). As médias vão primeiro
//    para uma fila em LittleFS (/queue.jsonl): nada se perde se a internet ou a
//    nuvem estiverem fora; o envio é retomado depois.
//  - Consulta da configuração do display e da calibração a cada minuto
//    (GET /api/v1/device/config), aplicada no loop principal.
// As chamadas HTTPS rodam numa tarefa própria (núcleo 0), sem travar o display.
// URL e token ficam na NVS (comandos "nuvem url" e "nuvem token").

struct UplinkStatus {
  bool configured = false;   // URL e token definidos
  uint32_t queue = 0;        // médias aguardando envio
  uint32_t lastOkEpoch = 0;  // último envio aceito (UTC)
  int lastHttp = 0;          // último status HTTP (ou código negativo de erro)
  String lastError;
  String url;
  uint32_t configVersion = 0;     // versão da configuração da nuvem aplicada
  uint32_t lastConfigEpoch = 0;   // última consulta de configuração bem-sucedida
};

void uplinkBegin();

// Chamar a cada leitura (5 s): acumula a média do intervalo de 10 min.
void uplinkAddReading(const Reading &r);

// Chamar no loop(): aplica a configuração recebida da nuvem.
void uplinkLoop();

// Cópia do estado (seguro para chamar de qualquer lugar).
UplinkStatus uplinkStatus();

bool uplinkSetUrl(const String &url, String &err);
bool uplinkSetToken(const String &token, String &err);
void uplinkDisable();        // apaga URL e token
void uplinkSendNow();        // força envio da fila e consulta da configuração
uint32_t uplinkClearQueue(); // descarta a fila; retorna quantas médias foram apagadas
