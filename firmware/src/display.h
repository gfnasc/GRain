#pragma once

#include "sensors.h"

// Inicializa o OLED (SSD1306 128x64 via I2C, mesmo barramento dos sensores).
bool displayBegin();

// Informa a leitura mais recente; a tela é redesenhada na hora.
void displayUpdate(const Reading &r);

// Chamar no loop(): troca de página a cada DISPLAY_PAGE_MS.
void displayLoop();
