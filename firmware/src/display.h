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
