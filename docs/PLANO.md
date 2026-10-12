# GWeather — Plano (v3)

Versão visual e detalhada (diagrama de ligações, checklist de montagem, calculadora de altitude):
[plano.html](plano.html) — baixe e abra no navegador.

## Escopo
- ESP32 DevKit v1 (30 pinos) + AHT20 + BMP280 (sem umidade) + OLED SSD1306 0,96" I2C. Sem botão.
- Montagem em protoboard, alimentação USB 5 V.
- WhatsApp via CallMeBot, mensagem individual para cada número cadastrado (3–4 mensagens/dia).
- Painel na rede local agora; expansão para Vercel planejada (fase 10).

## Ligações (todos os pinos usados ficam no lado direito da placa)
| ESP32 | Sinal | Destino | Fio |
|---|---|---|---|
| 3V3 | 3V3 | VIN/VCC dos 3 módulos | vermelho |
| GND (lado direito) | GND | GND dos 3 módulos | preto |
| D21 | SDA | SDA dos 3 módulos | azul |
| D22 | SCL | SCL dos 3 módulos | roxo |

BMP280: CSB → 3V3 com fio **laranja**; SDO → GND com fio **amarelo** (pinos de configuração, não de alimentação).

Endereços: AHT20 `0x38` · BMP280 `0x76` (SDO em GND; ID `0x58`) · OLED `0x3C`.
Atenção: no OLED o GND costuma vir antes do VCC. Nunca usar VIN (5 V).

## Altitude
`altitude = terreno + andar × altura_do_andar + altura_do_sensor` (valores em `firmware/include/site.h`)
`P_mar = P / (1 − alt/44330)^5,255`. Cada ~8 m de erro na altitude erra ~1 hPa. Conferir com o QNH do METAR do aeroporto mais próximo.

## Fases
1. ✅ Montagem na protoboard.
2. ✅ Ambiente PlatformIO, scanner I2C, leitura AHT20 + BMP280 e cálculos.
3. 🔧 OLED (U8g2), 3 páginas com rotação automática — implementada, aguardando conferência visual.
4. Wi-Fi (WiFiManager), NTP, mDNS `gweather.local`, config LittleFS + segredos NVS.
5. Servidor web + painel local + API `/api/v1`.
6. Logs (média 10 min, CSV mensal, UTC), gráficos, export.
7. Personalização do display + calibração por offset.
8. Previsão (Zambretti + Open-Meteo) + CallMeBot + agenda + alertas.
9. Robustez: watchdog, OTA, teste de 72 h.
10. Vercel: ingestão (ESP32 faz push), painel Next.js com login, config remota; agendamento continua no ESP32 (cron do plano Hobby roda 1×/dia).

## Bibliotecas
Adafruit AHTX0 · Adafruit BMP280 Library · Adafruit Unified Sensor · U8g2 · (próximas fases: ArduinoJson 7 · ESPAsyncWebServer · WiFiManager)
