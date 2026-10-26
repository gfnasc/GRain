#pragma once

#include <Arduino.h>

// Buzzer ativo no PIN_BUZZER: só bipes curtos, com ritmo dado por um temporizador (não pelo loop).
// Respeita buzzer.enabled; o horário silencioso (quiet) cala só os sons automáticos (alerta e
// bipe de boot) — clique e confirmação do botão tocam sempre. "force" ignora tudo (testes).
enum BuzzPattern : uint8_t { BZ_BOOT, BZ_CLICK, BZ_ALERT, BZ_CONFIRM };

void buzzerBegin();  // chamar logo no início do setup(), depois de settingsBegin()
void buzzerLoop();
void buzzerPlay(BuzzPattern p, bool force = false);
void buzzerStop();  // para o que estiver tocando e a repetição do alerta

// Alerta de chuva: 3 bipes curtos a cada 2 s, por até 30 s (ou até buzzerStop(), com o toque no
// botão, ou o alerta acabar). No horário silencioso não toca, salvo forçado (teste do alerta).
void buzzerAlertStart(bool force = false);

// Agora está no horário silencioso (configurado e com a hora sincronizada)?
bool quietNow();
