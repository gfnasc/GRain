# GRain (antigo GWeather) — Plano (v5)

Versão visual e detalhada (diagrama de ligações, checklist de montagem, calculadora de altitude):
[plano.html](plano.html) — baixe e abra no navegador.

## Escopo
- ESP32 DevKit v1 (30 pinos) + AHT20 + BMP280 (sem umidade) + OLED SSD1306 0,96" I2C. Sem botão.
- Montagem em protoboard, alimentação USB 5 V, estação no **escritório**.
- Foco: **previsão de chuva para 1 h, 2 h e 3 h**, cruzando o barômetro da estação com dados externos.
- Dois modos: **Outdoor** (lá fora: dados externos + pressão da estação) e **Indoor** (escritório: sensores da estação).
- **Configurações só no app do ESP32** (`grain.local`); **painel web em Nuxt na Vercel só para visualização**.
- WhatsApp (CallMeBot) adiado para a última fase.

## Arquitetura (v5)
```
ESP32 (escritório) ──médias de 10 min + cópia da config (HTTPS)──► Nuxt na Vercel (Nitro API + Postgres)
  │                                                                   │   ▲
  └─ grain.local: todas as configurações e controles                  │   └─ METAR + Open-Meteo
                                                                      └─ painel só leitura, com login
```
- O ESP32 sempre inicia a comunicação; nada de abrir porta no roteador. Ele não busca mais configuração na nuvem: é a única fonte de verdade e envia uma cópia junto com o estado.
- Cada envio da estação (a cada 10 min) é a “batida do relógio” que dispara a coleta externa e o cálculo da chuva (o cron do plano Hobby da Vercel roda só 1×/dia).
- Configurações só podem ser mudadas na rede de casa. O botão “Está chovendo aqui” fica no painel e na página local, porque registra uma observação e não altera configuração.

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

## Altitude e pressão
`altitude = terreno + andar × altura_do_andar + altura_do_sensor` (valores iniciais em `firmware/include/site.h`, depois em `/config.json`)
`P_mar = P / (1 − alt/44330)^5,255`. Cada ~8 m de erro na altitude erra ~1 hPa.
Na v5 (Fase 14), a nuvem calcula o desvio em relação ao QNH do aeroporto mais próximo (mediana de 7 dias) e devolve na resposta do envio; o ESP32 aplica se a chave “calibrar pressão pelo aeroporto” estiver ligada. Isso também corrige a altitude se o escritório ficar em outro andar ou endereço.

## Por que a reformulação (v5, 13/out/2026)
Medindo na sacada, a temperatura ficou de 3 a 4 °C acima à noite e até 10 °C acima com sol direto, e a umidade relativa ficou distorcida. O ponto de orvalho e a pressão (a mesma tendência, com desvio fixo de cerca de 2 hPa) continuam válidos. Por isso a estação foi para o escritório, a temperatura de fora passa a vir de dados externos e o barômetro vira a peça central da previsão de chuva.

- **Outdoor**: METAR do aeroporto mais próximo (detectado pela localização configurada no ESP32), Open-Meteo e a pressão da estação, que é a mesma dentro e fora de casa.
- **Indoor**: o escritório (AHT20), com conforto, ar seco, mofo, “abra a janela” e atraso térmico. O display continua com os dados da estação, sem página de previsão.
- Histórico da sacada mantido, com a flag **sacada** (tabela `placements`).

## Chuva em 1 h, 2 h e 3 h
Métodos do menos ao mais preciso, todos calculados a cada 10 min, registrados e pontuados:

| Método | Usa |
|---|---|
| M1 Barômetro | Tendência de 1 h e 3 h e Zambretti |
| M2 Barômetro + observação | M1 + METAR (T−Td, CB/TCU, teto, vento, VCSH/VCTS) + TAF |
| M3 Modelo numérico | Open-Meteo: probabilidade e volume por hora, dados de 15 em 15 min, CAPE |
| M4 Combinado | M1–M3 + hora do dia e época do ano; pesos ajustados pelo próprio histórico |
| M5 Radar (opcional) | Refletividade ao redor e deslocamento das células; fonte a definir |

O que conta como chuva observada: o METAR (RA/DZ/SHRA/TSRA), o botão “Está chovendo aqui” e, como reserva, o volume registrado pelo Open-Meteo. Placar por método e por janela (últimos 30 dias): Brier, acertos e alarmes falsos, e confiabilidade.

## Banco (mudanças da v5)
- Novas: `placements` (flag sacada/escritório), `outdoor_obs`, `forecast_snapshots`, `rain_predictions`, `rain_truth`, `devices.site` e `devices.station_config`.
- Removidas: `devices.config*`, `notify`, `recipients` e `notifications`.

## Fases
1. ✅ Montagem na protoboard.
2. ✅ Ambiente PlatformIO, scanner I2C, leitura AHT20 + BMP280 e cálculos.
3. ✅ OLED (U8g2): abertura com spinner (2,5 s) e páginas com rotação automática.
4. ✅ Wi-Fi (WiFiManager, senha aleatória na NVS), NTP, mDNS `gweather.local`, `/config.json` em LittleFS, console serial, página Rede e relógio no display.
5. ✅ API local `/api/v1` (now, status, config com PUT parcial e senha, reboot) e página mínima no ESP32; contrato em `docs/api-v1.md`; IPv6 para `gweather.local` responder rápido no macOS/iOS.
6. ✅ Nuvem: Nuxt na Vercel (`web/`), Postgres, `POST /api/v1/ingest` com token por aparelho, envio pelo ESP32 com reserva offline, painel com login.
7. ✅ Histórico, gráficos (24 h / 7 d / 30 d), mínimas e máximas, exportação CSV no painel.
8. ✅ Personalização do display pelo painel; ESP32 busca a configuração na nuvem. *(Na v5 o editor vai para o ESP32, Fase 12.)*
9. ⏹️ Previsão + WhatsApp: encerrada na v5 sem validação; código removido na Fase 11, volta como Fase 19.
10. ➡️ Robustez: movida para a Fase 18.

**v5 — GRain**

11. 🔧 (em validação) GRain, escritório e limpeza: renome (GitHub, Vercel com domínio novo e o antigo como alias, firmware, `grain.local`), `placements` com flag sacada, remoção do WhatsApp (aba, rotas, envio, agenda, alertas, tabelas). *Resultado: firmware 0.8.0 com `grain.local`; repositório `gfnasc/GRain`; painel no domínio novo (o antigo segue como alias); histórico da sacada hachurado e fora dos extremos; WhatsApp e suas tabelas removidos.*
12. Configurações só no ESP32: editor do display com prévia do OLED, calibração e localização na página local; ESP32 para de buscar config e envia uma cópia; painel somente leitura (a aba Display vira Estação).
13. Outdoor: METAR e Open-Meteo gravados em `outdoor_obs`, cards Outdoor × Indoor e previsão de hoje/amanhã na aba Agora, Indoor × Outdoor no histórico.
14. Pressão calibrada automaticamente pelo QNH (mediana de 7 dias), aplicada pelo ESP32; gráfico estação × QNH.
15. Chuva v1: M1–M3, registro, chuva observada, botão “Está chovendo aqui”, aba Chuva e placar.
16. Indoor: conforto, ar seco, mofo, janela, atraso térmico; resumo do dia com Indoor e Outdoor.
17. Chuva v2: M4 com aprendizado (após ~20–30 eventos de chuva) e M5 experimental.
18. Robustez: watchdog, OTA, reinício sem rede, aviso de erro no OLED, teste de 72 h.
19. WhatsApp: números, chaves e horários configurados no ESP32; mensagens Outdoor/Indoor/Chuva montadas pela nuvem e alerta de chuva.

## Bibliotecas
Firmware: Adafruit AHTX0 · Adafruit BMP280 Library · Adafruit Unified Sensor · U8g2 · ArduinoJson 7 · WiFiManager · (WebServer e HTTPClient do core ESP32)
Web: Nuxt + Nitro na Vercel · Postgres (Neon) · uPlot
