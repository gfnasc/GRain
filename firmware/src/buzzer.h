#pragma once

#include <Arduino.h>

// Buzzer ativo no PIN_BUZZER: só bipes curtos, com ritmo dado por um temporizador (não pelo loop).
// Respeita buzzer.enabled; o horário silencioso (quiet) cala só os sons automáticos (alerta e
// bipe de boot) — clique e confirmação do botão tocam sempre. "force" ignora tudo (testes).
enum BuzzPattern : uint8_t { BZ_BOOT, BZ_CLICK, BZ_ALERT, BZ_CONFIRM };

void buzzerBegin();  // chamar logo no início do setup(), depois de settingsBegin()
void buzzerLoop();
void buzzerPlay(BuzzPattern p, bool force = false);
void buzzerStop();

// Agora está no horário silencioso (configurado e com a hora sincronizada)?
bool quietNow();
