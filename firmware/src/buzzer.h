#pragma once

#include <Arduino.h>

// Buzzer ativo no PIN_BUZZER: só bipes curtos, tocados sem travar o loop.
// Respeita buzzer.enabled e o horário silencioso (quiet), salvo quando forçado (testes).
enum BuzzPattern : uint8_t { BZ_BOOT, BZ_CLICK, BZ_ALERT, BZ_CONFIRM };

void buzzerBegin();  // chamar logo no início do setup(), depois de settingsBegin()
void buzzerLoop();
void buzzerPlay(BuzzPattern p, bool force = false);
void buzzerStop();

// Agora está no horário silencioso (configurado e com a hora sincronizada)?
bool quietNow();
