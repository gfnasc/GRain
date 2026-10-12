# Hardware — GWeather

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

## Periféricos e sensores
| Item | Interface | Endereço/Pino | Observação |
|---|---|---|---|
| AHT20 | I2C | 0x38 | temperatura (principal) e umidade |
| BMP280 | I2C | 0x76 (chip id 0x58) | pressão; CSB → 3V3 (fio laranja), SDO → GND (fio amarelo) |
| OLED SSD1306 128×64 | I2C | 0x3C | 3 páginas rotativas |

## Restrições conhecidas
- Tudo em 3,3 V; nunca usar VIN (5 V).
- I2C a 100 kHz (`I2C_FREQ_HZ`); pode subir para 400 kHz se precisar.
- ESP32 esquenta com Wi-Fi: sensores ficam na ponta oposta da protoboard.
- Altitude do sensor vem de `include/site.h` (local, fora do git).
