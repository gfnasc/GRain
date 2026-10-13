#pragma once

#include <Arduino.h>

// Robustez: watchdog, atualização pelo Wi-Fi (OTA), autocura e diagnóstico.
//  - Watchdog: se o loop principal (ou a tarefa da nuvem) ficar WDT_TIMEOUT_S sem avisar, reinicia.
//  - OTA: ArduinoOTA na porta 3232, protegido pela senha da estação; o display mostra o progresso.
//  - Autocura: reinicia após NET_RESTART_AFTER_MS sem Wi-Fi ou CLOUD_RESTART_AFTER_MS com a nuvem
//    falhando (com Wi-Fi), e guarda o motivo para o próximo boot.
//  - Diagnóstico: motivo do último reinício e contador de reinícios (NVS).

void healthBegin();  // no início do setup()
void healthLoop();   // a cada volta do loop()

const char *healthResetReason();  // ex.: "energia", "watchdog", "sem Wi-Fi por 30 min"
uint32_t healthBoots();           // reinícios desde a primeira gravação deste recurso
bool healthOtaActive();           // atualização em andamento (o resto pausa)

// Reinicia guardando o motivo (aparece como "último reinício" depois).
void healthRestart(const char *why);
