# Status

Memória de continuidade entre sessões. O Claude lê isto ao começar e **atualiza ao terminar** cada tarefa.
Plano geral: `../../docs/PLANO.md` e `../../docs/plano.html`.

## Estado atual
- Fase 1 (montagem) concluída.
- Fase 2 (ambiente, scanner I2C, sensores) testada na placa:
  - Scanner encontra 0x38 (AHT20), 0x3C (OLED), 0x76 (BMP280, chip id 0x58).
  - Leitura a cada 5 s: temperatura/umidade (AHT20), temperatura/pressão (BMP280),
    pressão ao nível do mar, ponto de orvalho, sensação térmica.
  - Primeira leitura (ambiente interno): 28,1 °C, 65 %, 1013,4 hPa ao nível do mar; BMP − AHT = −0,07 °C.
- Fase 3 (OLED) implementada e gravada: `src/display.*` com U8g2 em buffer completo, 3 páginas
  (Temperatura, Pressão, Detalhes) trocando a cada 5 s, redesenho a cada nova leitura, indicador de página,
  vírgula decimal. Log confirma "OLED ok em 0x3C".
- Configuração do local separada em `include/site.h` (fora do git; modelo em `include/site.h.example`).
- Módulos: `src/i2c_scan.*`, `src/sensors.*` (reinicializa sensor após 3 falhas), `src/weather_math.*` (funções puras), `src/display.*`.
- `dev` e `release` compilam.

## Em andamento
- Fase 3: aguardando confirmação visual do OLED (páginas legíveis, sem piscar).

## Próximos passos
1. Confirmar o OLED e que a umidade sobe ao soprar no AHT20 (pendente da fase 2).
2. Fase 4: Wi-Fi (WiFiManager), NTP, mDNS `gweather.local`, config em LittleFS + segredos na NVS.

## Problemas conhecidos
- Log `[W][Wire.cpp] Bus already started in Master Mode` no boot: as bibliotecas chamam `Wire.begin()` de novo. Inofensivo.
- Altitude do terreno em `site.h` é estimada; conferir com a altitude da rua e com o QNH do METAR do aeroporto mais próximo.
