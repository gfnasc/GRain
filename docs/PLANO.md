# GRain (antigo GWeather) — Plano (v5)

Versão visual e detalhada (diagrama de ligações, checklist de montagem, calculadora de altitude):
[plano.html](plano.html) — baixe e abra no navegador.

## Escopo
- ESP32 DevKit v1 (30 pinos) + AHT20 + BMP280 (sem umidade) + OLED SSD1306 0,96" I2C. Sem botão.
- Montagem em protoboard, alimentação USB 5 V.
- Painel em **Nuxt na Vercel**; ESP32 com API local e página mínima.
- WhatsApp via CallMeBot, mensagem individual para cada número cadastrado (3–4 mensagens/dia).

## Arquitetura (decidida na v4)
```
ESP32 ──envia leituras a cada 10 min (HTTPS)──► Nuxt na Vercel (Nitro API + Postgres)
  │                                                   │
  └─ página mínima em gweather.local                  └─ painel completo com login
```
- O painel não fica no ESP32: um site HTTPS não pode ler a API HTTP local, `gweather.local` só existe dentro de casa e o LittleFS (1 MB) fica livre para a reserva de dados.
- O ESP32 sempre inicia a comunicação (envia dados, busca configuração); nada de abrir porta no roteador.
- Agendamento do WhatsApp continua no ESP32 (cron do plano Hobby da Vercel roda só 1×/dia).

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
`altitude = terreno + andar × altura_do_andar + altura_do_sensor` (valores iniciais em `firmware/include/site.h`, depois em `/config.json`)
`P_mar = P / (1 − alt/44330)^5,255`. Cada ~8 m de erro na altitude erra ~1 hPa. Conferir com o QNH do METAR do aeroporto mais próximo.

## Fases
1. ✅ Montagem na protoboard.
2. ✅ Ambiente PlatformIO, scanner I2C, leitura AHT20 + BMP280 e cálculos.
3. ✅ OLED (U8g2): abertura com spinner (2,5 s) e páginas com rotação automática.
4. ✅ Wi-Fi (WiFiManager, senha aleatória na NVS), NTP, mDNS `gweather.local`, `/config.json` em LittleFS, console serial, página Rede e relógio no display.
5. ✅ API local `/api/v1` (now, status, config com PUT parcial e senha, reboot) e página mínima no ESP32; contrato em `docs/api-v1.md`; IPv6 para `gweather.local` responder rápido no macOS/iOS.
6. ✅ Nuvem: Nuxt na Vercel (`web/`), Postgres, `POST /api/v1/ingest` com token por aparelho, envio pelo ESP32 com reserva offline, painel com login.
7. ✅ Histórico, gráficos (24 h / 7 d / 30 d), mínimas e máximas, exportação CSV no painel.
8. ✅ Personalização do display pelo painel; ESP32 busca a configuração na nuvem.
9. ⏹️ Previsão + WhatsApp: encerrada na v5 sem validação; código removido na Fase 11, volta como Fase 19.
10. ➡️ Movida para a Fase 18.

## Reformulação v5 (aprovada em 13/out/2026)
Novo nome: **GRain** (Guilherme + Rain). Medindo na sacada, a temperatura ficou de 3 a 4 °C acima à noite e até 10 °C acima com sol direto, e a umidade relativa ficou distorcida. O ponto de orvalho e a pressão (a mesma tendência, com desvio fixo de cerca de 2 hPa) continuam válidos. Por isso a estação vai para o escritório e o projeto passa a ter dois modos:

- **Outdoor**: METAR do aeroporto mais próximo (detectado pela localização configurada no ESP32), Open-Meteo e a pressão da estação, que é a mesma dentro e fora de casa.
- **Indoor**: o escritório (AHT20), com conforto, ar seco, mofo, “abra a janela” e atraso térmico. O display continua com os dados da estação, sem página de previsão.

**Regras novas**
- Todas as configurações ficam no app do ESP32 (`grain.local`): display, calibração, localização, rede e nuvem. O ESP32 não busca mais configuração na nuvem; ele envia uma cópia junto com o estado.
- O painel web serve só para ver os dados. O botão “Está chovendo aqui” continua lá, porque registra uma observação e não altera configuração.
- O WhatsApp sai agora (código e tabelas removidos) e volta como a última fase.

**Chuva em 1 h, 2 h e 3 h**, com métodos do menos ao mais preciso, todos registrados e pontuados:

| Método | Usa |
|---|---|
| M1 Barômetro | Tendência de 1 h e 3 h e Zambretti |
| M2 Barômetro + observação | M1 + METAR (T−Td, CB/TCU, teto, vento, VCSH/VCTS) + TAF |
| M3 Modelo numérico | Open-Meteo: probabilidade e volume por hora, dados de 15 em 15 min, CAPE |
| M4 Combinado | M1–M3 + hora do dia e época do ano; pesos ajustados pelo próprio histórico |
| M5 Radar (opcional) | Refletividade ao redor e deslocamento das células; fonte a definir |

O que conta como chuva observada: o METAR (RA/DZ/SHRA/TSRA), o botão “Está chovendo aqui” e, como reserva, o volume registrado pelo Open-Meteo. Placar por método e por janela: Brier, acertos e alarmes falsos, e confiabilidade.

A coleta externa é disparada por cada envio da estação (a cada 10 min). Desvio de pressão em relação ao QNH: a nuvem calcula e devolve na resposta do envio, e o ESP32 aplica se a chave estiver ligada.

**Banco**
- Novas: `placements` (flag sacada/escritório), `outdoor_obs`, `forecast_snapshots`, `rain_predictions`, `rain_truth`, `devices.site` e `devices.station_config`.
- Removidas: `devices.config*`, `notify`, `recipients` e `notifications`.

### Fases da v5
11. GRain, escritório e limpeza: renome (GitHub, Vercel, firmware, `grain.local`), `placements` com flag sacada, remoção do WhatsApp.
12. Configurações só no ESP32: editor do display com prévia, calibração e localização na página local; painel somente leitura (a aba Display vira Estação).
13. Outdoor: METAR e Open-Meteo, cards Outdoor × Indoor e previsão na aba Agora, Indoor × Outdoor no histórico.
14. Pressão calibrada automaticamente pelo QNH (mediana de 7 dias).
15. Chuva v1: M1–M3, registro, chuva observada, botão “Está chovendo aqui”, aba Chuva e placar.
16. Indoor: conforto, ar seco, mofo, janela, atraso térmico.
17. Chuva v2: M4 com aprendizado e M5 experimental.
18. Robustez: watchdog, OTA, teste de 72 h.
19. WhatsApp: configurado no ESP32, mensagens Outdoor/Indoor/Chuva e alerta de chuva.

## Bibliotecas
Firmware: Adafruit AHTX0 · Adafruit BMP280 Library · Adafruit Unified Sensor · U8g2 · ArduinoJson 7 · WiFiManager · (WebServer e HTTPClient do core ESP32)
Web (fase 6): Nuxt + Nitro na Vercel · Postgres (Neon)
