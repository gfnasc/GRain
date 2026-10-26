# Hardware — GWethr

Fonte de verdade do hardware. Atualize sempre que a fiação mudar e mantenha `include/config.h` sincronizado.
Diagrama visual: `../../docs/plano.html` (seção Ligações).

- Placa: `esp32dev` (ESP32 DevKit v1, 30 pinos, USB CP2102)
- Porta serial: detectada automaticamente pelo PlatformIO (no macOS costuma ser `/dev/cu.usbserial-*`)
- Alimentação: USB 5 V (fonte de 1 A para uso contínuo)
- Montagem: protoboard, sem caixa

## Pinagem
| Função | GPIO | Direção | Observação |
|---|---|---|---|
| I2C SDA | 21 | bidirecional | fio azul; compartilhado por AHT20, BMP280, OLED |
| I2C SCL | 22 | saída | fio roxo; compartilhado por AHT20, BMP280, OLED |
| LED | 2 | saída | LED azul onboard; hoje pisca a cada leitura; na Fase 19 vira retorno visual (botão, alerta, falhas, OTA) |
| 3V3 | – | alimentação | trilho +, fio vermelho |
| GND | – | alimentação | trilho −, fio preto (GND do lado direito) |
| Botão (Fase 19, ligado) | 19 | entrada | para o GND, pull-up interno |
| Buzzer ativo (Fase 19, planejado) | 18 | saída | módulo em 3V3 (nunca VIN); conferir se aciona em nível baixo; 10 kΩ I/O→3V3 evita bipe no boot |

## Periféricos e sensores
| Item | Interface | Endereço/Pino | Observação |
|---|---|---|---|
| AHT20 | I2C | 0x38 | temperatura (principal) e umidade |
| BMP280 | I2C | 0x76 (chip id 0x58) | pressão; CSB → 3V3 (fio laranja), SDO → GND (fio amarelo) |
| OLED SSD1306 128×64 | I2C | 0x3C | 3 páginas rotativas |

## Rede e memória
- Wi-Fi 2,4 GHz (WiFiManager); rede de configuração "GWethr-Setup" com senha aleatória mostrada no OLED.
- mDNS: `gwethr.local` (os antigos `grain` e `gweather` são migrados sozinhos).
- Local: escritório (antes, sacada). Na sacada a temperatura e a umidade eram distorcidas por sol e calor do prédio.
- Flash 4 MB (`partitions.csv`): app0/app1 de 1,44 MB cada (OTA), LittleFS 1 MB, coredump 64 KB.

## Restrições conhecidas
- Tudo em 3,3 V; nunca usar VIN (5 V).
- I2C a 100 kHz (`I2C_FREQ_HZ`); pode subir para 400 kHz se precisar.
- ESP32 esquenta com Wi-Fi: sensores ficam na ponta oposta da protoboard.
- Altitude do sensor vem de `include/site.h` (local, fora do git).
