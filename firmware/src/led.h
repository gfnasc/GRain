#pragma once

// LED azul onboard (PIN_LED) como retorno visual, por prioridade:
//   rede de configuração (fixo) > botão apertado (fixo; pisca rápido nos 2 s; fixo nos 5 s) >
//   aviso de chuva (pisca devagar até a nuvem aceitar; 2 piscadas quando aceita) >
//   alerta de chuva ativo e não calado (piscada dupla a cada 2 s) > falha (tripla a cada 5 s) >
//   sinal de vida (piscada curta a cada leitura).
// A gravação pelo Wi-Fi pisca rápido pelo progresso (health.cpp).
// led.mode: normal, só avisos (sem sinal de vida) ou desligado; no horário silencioso vale led.quiet.
void ledBegin();
void ledLoop();
void ledPulse();  // sinal de vida: chamar a cada leitura
