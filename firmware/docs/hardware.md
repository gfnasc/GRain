# Hardware — GRain

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
| LED | 2 | saída | LED onboard, pisca a cada leitura |
| 3V3 | – | alimentação | trilho +, fio vermelho |
| GND | – | alimentação | trilho −, fio preto (GND do lado direito) |
| Botão (Fase 19, planejado) | 4 | entrada | para o GND, pull-up interno; GPIO 4 também é toque capacitivo (T0) |
| LED vermelho (planejado) | 19 | saída | resistor de 220–330 Ω até o GND |
| LED amarelo (planejado) | 18 | saída | resistor de 220–330 Ω até o GND |
| LED verde (planejado) | 17 | saída | resistor de 220–330 Ω até o GND (pino TX2, UART2 não usada) |
| Buzzer (planejado) | 16 | saída | módulo com transistor em 3V3 (pino RX2, UART2 não usada) |

## Periféricos e sensores
| Item | Interface | Endereço/Pino | Observação |
|---|---|---|---|
| AHT20 | I2C | 0x38 | temperatura (principal) e umidade |
| BMP280 | I2C | 0x76 (chip id 0x58) | pressão; CSB → 3V3 (fio laranja), SDO → GND (fio amarelo) |
| OLED SSD1306 128×64 | I2C | 0x3C | 3 páginas rotativas |

## Rede e memória
- Wi-Fi 2,4 GHz (WiFiManager); rede de configuração "GRain-Setup" com senha aleatória mostrada no OLED.
- mDNS: `grain.local` (o hostname antigo `gweather` é migrado sozinho).
- Local: escritório (antes, sacada). Na sacada a temperatura e a umidade eram distorcidas por sol e calor do prédio.
- Flash 4 MB (`partitions.csv`): app0/app1 de 1,44 MB cada (OTA), LittleFS 1 MB, coredump 64 KB.

## Restrições conhecidas
- Tudo em 3,3 V; nunca usar VIN (5 V).
- I2C a 100 kHz (`I2C_FREQ_HZ`); pode subir para 400 kHz se precisar.
- ESP32 esquenta com Wi-Fi: sensores ficam na ponta oposta da protoboard.
- Altitude do sensor vem de `include/site.h` (local, fora do git).
