#pragma once

// Pinos e constantes. Mantenha docs/hardware.md sincronizado.

// Barramento I2C (AHT20, BMP280 e OLED compartilham)
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_FREQ_HZ 100000

// Endereços I2C esperados
#define ADDR_AHT20 0x38
#define ADDR_BMP280 0x76      // SDO em GND; 0x77 é tentado como alternativa
#define ADDR_BMP280_ALT 0x77
#define ADDR_OLED 0x3C

// LED onboard (usado como sinal de vida)
#define PIN_LED 2

// Leitura
#define SENSOR_INTERVAL_MS 5000
#define SENSOR_REINIT_AFTER_FAILS 3

// Local de instalação (altitude): fica em include/site.h, fora do git.
#if __has_include("site.h")
#include "site.h"
#else
#error "Falta include/site.h: copie include/site.h.example para include/site.h e ajuste a altitude."
#endif

// Display OLED
#define DISPLAY_PAGE_MS 5000
#define DISPLAY_CONTRAST 128

// Diferença de temperatura AHT20 x BMP280 acima da qual avisamos
#define TEMP_MISMATCH_WARN_C 2.0f
