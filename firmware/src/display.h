#pragma once

#include "sensors.h"

// Inicializa o OLED (SSD1306 128x64 via I2C, mesmo barramento dos sensores).
bool displayBegin();

// Informa a leitura mais recente; a tela é redesenhada na hora.
void displayUpdate(const Reading &r);

// Chamar no loop(): troca de página no intervalo configurado e mostra a
// tela de configuração do Wi-Fi enquanto ela estiver aberta.
void displayLoop();

// Reaplica intervalo de página e contraste após mudança na configuração.
void displayApplySettings();

// --- Controles manuais (página local / console). Não são gravados: valem até reiniciar. ---
struct DisplayState {
  bool on;         // tela ligada (false = desligada manualmente)
  bool paused;     // carrossel pausado
  bool night;      // modo noturno ativo agora
  int page;        // página atual (0..count-1)
  int count;       // número de páginas
  uint32_t holdS;  // segundos até o carrossel voltar a girar após navegação manual (0 = girando)
};
DisplayState displayState();
void displaySetPower(bool on);
void displaySetPaused(bool paused);
// Avança (+1) ou volta (-1) uma página; segura a página por 60 s se o carrossel estiver girando.
void displayStep(int delta);
// Vai direto para a página (0..count-1), com a mesma espera de 60 s.
bool displayGoto(int index);

// Tela de atualização pelo Wi-Fi: progresso (0..100) ou erro (pct < 0) com a mensagem.
void displayOta(int pct, const char *msg);

// --- Alerta de chuva (previsão da nuvem + limite configurado) ---
// Ativo agora? h = janela (1..3 h) e p = chance (%) que dispararam.
bool displayRainAlert(int *h, int *p);
// Mostra o alerta por 12 s com valores de exemplo (botão "Testar alerta").
void displayAlertTest();
