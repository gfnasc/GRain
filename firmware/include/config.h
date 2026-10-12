#pragma once

// Pinos e constantes. Mantenha docs/hardware.md sincronizado.

#define FW_VERSION "0.7.1"

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
#ifndef SITE_TZ
#define SITE_TZ "UTC0"
#endif

// Display OLED
#define DISPLAY_PAGE_MS 5000
#define DISPLAY_CONTRAST 128
#define DISPLAY_SPLASH_MS 2500         // tela "Iniciando..." no boot
#define DISPLAY_SPINNER_FRAME_MS 100   // velocidade do spinner

// Rede
#define DEFAULT_HOSTNAME "gweather"        // acesso por gweather.local
#define DEFAULT_NTP1 "pool.ntp.org"
#define DEFAULT_NTP2 "time.google.com"
#define PORTAL_AP_NAME "GWeather-Setup"    // rede de configuração do Wi-Fi
#define PORTAL_TIMEOUT_S 180               // só quando já existe Wi-Fi salvo
#define WIFI_RETRY_MS 60000
#define API_USER "admin"                  // usuário das rotas que alteram algo

// Nuvem
#define UPLINK_INTERVAL_S 600              // médias de 10 min
#define UPLINK_BATCH_MAX 48                // médias por envio (8 h)
#define UPLINK_QUEUE_MAX 2016              // reserva offline: 14 dias
#define UPLINK_RETRY_MS 600000
#define UPLINK_BACKOFF_MAX_MS 1800000      // espera máxima entre tentativas com falha
#define CLOUD_CONFIG_POLL_MS 60000         // consulta da configuração do display na nuvem

// Diferença de temperatura AHT20 x BMP280 acima da qual avisamos
#define TEMP_MISMATCH_WARN_C 2.0f
