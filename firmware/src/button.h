#pragma once

#include <Arduino.h>

// Botão no PIN_BUTTON (para o GND, pull-up interno), com debounce. Três gestos, decididos ao soltar:
//   toque (< 1 s), segurar 2 s ("Está chovendo aqui"), segurar 5 s (liga/desliga a tela).
// Entre 1 e 2 s, ou depois de 10 s, soltar não faz nada.
enum BtnEvent : uint8_t { BTN_NONE, BTN_TAP, BTN_RAIN, BTN_SCREEN };

void buttonBegin();
BtnEvent buttonPoll();     // chamar no loop(); devolve o gesto quando o botão é solto
uint32_t buttonHeldMs();   // há quanto tempo está apertado (0 = solto)

// Último gesto reconhecido (para a página local): nome e hora (epoch, 0 = nunca).
const char *buttonLastName();
uint32_t buttonLastEpoch();
