#pragma once

// Pinos e constantes. Mantenha docs/hardware.md sincronizado.

#define FW_VERSION "0.15.3"

// Barramento I2C (AHT20, BMP280 e OLED compartilham)
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_FREQ_HZ 100000

// Endereços I2C esperados
#define ADDR_AHT20 0x38
#define ADDR_BMP280 0x76      // SDO em GND; 0x77 é tentado como alternativa
#define ADDR_BMP280_ALT 0x77
#define ADDR_OLED 0x3C

// LED azul onboard: retorno visual (botão, avisos, falhas, gravação pelo Wi-Fi, sinal de vida)
#define PIN_LED 2

// Interface física (Fase 19). Nunca no barramento I2C (D21/D22).
#define PIN_BUTTON 19   // botão para o GND, pull-up interno
#define PIN_BUZZER 18   // buzzer ativo (o nível que liga é configurável: buzzer.level)

// Botão: limites dos gestos (ms)
#define BTN_DEBOUNCE_MS 25
#define BTN_TAP_MAX_MS 1000      // soltar antes disso = toque
#define BTN_RAIN_MS 2000         // segurar 2 s = "Está chovendo aqui"
#define BTN_SCREEN_MS 5000       // segurar 5 s = liga/desliga a tela
#define BTN_CANCEL_MS 10000      // segurou demais (ou travou): não faz nada
#define DISPLAY_WAKE_MS 15000    // toque com a tela apagada: acende por 15 s
#define DISPLAY_TAP_HOLD_MS 30000  // modo "as duas": o toque segura a página por 30 s

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
#define DEFAULT_HOSTNAME "gwethr"          // acesso por gwethr.local
#define LEGACY_HOSTNAME "grain"            // nomes antigos (GRain, GWeather): migrados para DEFAULT_HOSTNAME ao carregar
#define LEGACY_HOSTNAME2 "gweather"
#define DEFAULT_NTP1 "pool.ntp.org"
#define DEFAULT_NTP2 "time.google.com"
#define PORTAL_AP_NAME "GWethr-Setup"       // rede de configuração do Wi-Fi
#define PORTAL_TIMEOUT_S 180               // só quando já existe Wi-Fi salvo
#define WIFI_RETRY_MS 60000
#define API_USER "admin"                  // usuário das rotas que alteram algo

// Nuvem
#define UPLINK_INTERVAL_S 300              // médias de 5 min
#define UPLINK_BATCH_MAX 48                // médias por envio (4 h)
#define UPLINK_QUEUE_MAX 4032              // reserva offline: 14 dias
#define UPLINK_RETRY_MS 600000
#define UPLINK_BACKOFF_MAX_MS 1800000      // espera máxima entre tentativas com falha

// Robustez
#define WDT_TIMEOUT_S 60                    // loop ou tarefa da nuvem parados por mais que isso: reinicia
#define NET_RESTART_AFTER_MS (30UL * 60 * 1000)        // sem Wi-Fi por 30 min: reinicia
#define CLOUD_RESTART_AFTER_MS (6UL * 60 * 60 * 1000)  // nuvem falhando por 6 h (com Wi-Fi): reinicia

// Diferença de temperatura AHT20 x BMP280 acima da qual avisamos
#define TEMP_MISMATCH_WARN_C 2.0f
