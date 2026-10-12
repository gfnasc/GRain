#pragma once

#include <Arduino.h>

#include "sensors.h"

// Envio das médias de 10 min para a nuvem (POST /api/v1/ingest).
// As médias vão primeiro para uma fila em LittleFS (/queue.jsonl), então
// nada se perde se a internet ou a nuvem estiverem fora: o envio é retomado
// depois. URL e token ficam na NVS (comandos "nuvem url" e "nuvem token").

struct UplinkStatus {
  bool configured = false;  // URL e token definidos
  uint32_t queue = 0;       // médias aguardando envio
  uint32_t lastOkEpoch = 0; // último envio aceito (UTC)
  int lastHttp = 0;         // último status HTTP (ou código negativo de erro)
  String lastError;
  String url;
};

void uplinkBegin();

// Chamar a cada leitura (5 s): acumula a média do intervalo de 10 min.
void uplinkAddReading(const Reading &r);

// Chamar no loop(): envia a fila quando houver rede e chegar a hora.
void uplinkLoop();

const UplinkStatus &uplinkStatus();

bool uplinkSetUrl(const String &url, String &err);
bool uplinkSetToken(const String &token, String &err);
void uplinkDisable();        // apaga URL e token
void uplinkSendNow();        // força uma tentativa de envio
uint32_t uplinkClearQueue(); // descarta a fila; retorna quantas médias foram apagadas
