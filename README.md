# GWeather

Estação meteorológica com **ESP32 DevKit v1**, sensores **AHT20** e **BMP280** e display **OLED SSD1306**. Mede temperatura, umidade e pressão, mostra os dados no display e, nas próximas fases, terá painel web, histórico, mensagens de WhatsApp e publicação na nuvem.

> *ESP32 weather station (AHT20 + BMP280 + SSD1306 OLED). Documentation in Portuguese.*

> [!IMPORTANT]
> **Projeto pessoal, publicado como está.** O código foi escrito e testado para **um hardware específico**: a combinação exata de placa, sensores, display e ligações descrita abaixo. Para usar sem alterações, você precisa da **mesma configuração de módulos e componentes**. Outras placas, sensores ou displays exigem adaptar o código.
>
> O repositório não tem suporte: issues estão desativadas e não há garantia de funcionamento.

---

## Situação atual

| Fase | Descrição | Situação |
|---|---|---|
| 1 | Montagem na protoboard | ✅ Concluída |
| 2 | Scanner I2C e leitura dos sensores | ✅ Concluída |
| 3 | Display OLED com páginas rotativas | 🔧 Implementada, em validação |
| 4 | Wi-Fi, hora pela internet (NTP), `gweather.local` | Planejada |
| 5 | Servidor web e painel local | Planejada |
| 6 | Registro de logs, gráficos e exportação CSV | Planejada |
| 7 | Personalização do display pela web | Planejada |
| 8 | Previsão do dia e mensagens de WhatsApp (CallMeBot) | Planejada |
| 9 | Robustez: watchdog, OTA, teste de 72 h | Planejada |
| 10 | Painel na nuvem (Vercel) | Planejada |

Plano completo: [docs/PLANO.md](docs/PLANO.md). Versão visual, com diagrama interativo de ligações e montagem passo a passo: [docs/plano.html](docs/plano.html) (baixe o arquivo e abra no navegador).

---

## Hardware necessário

Use **exatamente** estes componentes:

| Componente | Especificação exigida | Observações |
|---|---|---|
| Placa | **ESP32 DevKit v1** (ESP-WROOM-32), 30 pinos | A versão de 38 pinos também serve, seguindo os **nomes** dos pinos, não a posição |
| Sensor de temperatura e umidade | **AHT20**, módulo I2C | Endereço `0x38` |
| Sensor de pressão | **BMP280**, módulo I2C | Endereço `0x76` (SDO em GND). Chip id `0x58` |
| Display | **OLED 0,96" SSD1306 128×64, I2C de 4 pinos** | Endereço `0x3C` |
| Protoboard | 830 furos (ou duas de 400) | O ESP32 é largo; veja as dicas de montagem no plano |
| Jumpers | macho-macho | Cores sugeridas abaixo |
| Cabo USB | **de dados** | Cabo só de carga não grava o firmware |
| Fonte | USB 5 V / 1 A | Para uso contínuo |

### O que **não** funciona sem mudar o código
- **BME280 no lugar do BMP280:** é detectado e funciona, mas a umidade dele é ignorada. A umidade vem só do AHT20.
- **OLED de 1,3" (driver SH1106)** ou **OLED SPI de 7 pinos:** usam outro driver ou outra ligação.
- **ESP32-S3, ESP32-C3 ou outras placas:** os pinos I2C e a configuração `board` do PlatformIO são diferentes.
- **DHT11/DHT22, BME680, SHT3x e outros sensores:** usam bibliotecas diferentes.

---

## Ligações

Os três módulos compartilham o mesmo barramento I2C, com 4 sinais. Todos os pinos usados ficam no **lado direito** do ESP32 (vista de cima, antena para cima).

| Pino do ESP32 | Sinal | Liga em | Cor sugerida |
|---|---|---|---|
| `3V3` | 3,3 V | VIN/VCC do AHT20, do BMP280 e do OLED | vermelho |
| `GND` (lado direito) | terra | GND do AHT20, do BMP280 e do OLED | preto |
| `D21` (GPIO21) | SDA | SDA dos três módulos | azul |
| `D22` (GPIO22) | SCL | SCL dos três módulos | roxo |

Ligações extras do BMP280 (pinos de configuração; se o seu módulo tiver só 4 pinos, ignore):

| Pino do BMP280 | Liga em | Cor sugerida | Função |
|---|---|---|---|
| `CSB` | 3V3 | laranja | Força o modo I2C |
| `SDO` | GND | amarelo | Fixa o endereço em `0x76` |

```
ESP32 3V3 ──┬── AHT20 VIN ── BMP280 VCC ── BMP280 CSB ── OLED VCC
ESP32 GND ──┼── AHT20 GND ── BMP280 GND ── BMP280 SDO ── OLED GND
ESP32 D21 ──┼── AHT20 SDA ── BMP280 SDA ── OLED SDA
ESP32 D22 ──┴── AHT20 SCL ── BMP280 SCL ── OLED SCL
```

> [!WARNING]
> - **Leia a serigrafia de cada módulo.** A ordem dos pinos muda entre fabricantes; no OLED é comum o **GND vir antes do VCC**. Inverter os dois pode queimar o display.
> - **Use só 3V3.** Nunca ligue os módulos no pino `VIN` (5 V).
> - Deixe AHT20 e BMP280 **longe do ESP32** (na outra ponta da protoboard). Com o Wi-Fi ligado, ele esquenta e altera a leitura de temperatura.

---

## Software

### Requisitos
- [VS Code](https://code.visualstudio.com/) com a extensão [PlatformIO IDE](https://platformio.org/install/ide?install=vscode), ou o [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation/index.html).
- Driver USB da sua placa, se o sistema não reconhecer: CP210x ou CH340.

As bibliotecas são instaladas automaticamente pelo PlatformIO, com versões fixadas em [`firmware/platformio.ini`](firmware/platformio.ini):

| Biblioteca | Versão |
|---|---|
| Adafruit AHTX0 | 2.0.6 |
| Adafruit BMP280 Library | 2.6.8 |
| Adafruit Unified Sensor | 1.1.15 |
| U8g2 | 2.36.18 |

### Instalação

1. Clone o repositório:
   ```bash
   git clone https://github.com/gfnasc/GWeather.git
   cd GWeather/firmware
   ```
2. Crie o arquivo de configuração do local a partir do modelo:
   ```bash
   cp include/site.h.example include/site.h
   ```
3. Edite `include/site.h` com a altitude do **seu** local (veja a seção abaixo).
4. Conecte o ESP32 pela USB e grave:
   ```bash
   pio run -e dev -t upload
   ```
5. Abra o monitor serial:
   ```bash
   pio device monitor
   ```

Se houver mais de uma porta serial, o PlatformIO pergunta qual usar. Para fixar a porta, descomente `upload_port` e `monitor_port` em `platformio.ini`.

### Configuração do local (`include/site.h`)

O BMP280 mede a pressão onde está. Para converter em pressão ao nível do mar, o firmware precisa da altitude real do sensor:

```
altitude = SITE_GROUND_ALT_M + SITE_FLOOR × SITE_FLOOR_HEIGHT_M + SITE_SENSOR_HEIGHT_M
```

| Constante | Significado | Exemplo |
|---|---|---|
| `SITE_GROUND_ALT_M` | Altitude da rua, em metros | `600.0f` |
| `SITE_FLOOR` | Andar do prédio (0 para casa ou térreo) | `10` |
| `SITE_FLOOR_HEIGHT_M` | Altura de cada andar, piso a piso | `2.9f` |
| `SITE_SENSOR_HEIGHT_M` | Altura do sensor acima do piso | `1.0f` |

Cada ~8 m de erro na altitude erra a pressão corrigida em ~1 hPa. Para conferir, compare com o QNH do METAR do aeroporto mais próximo.

O arquivo `site.h` fica fora do git (`.gitignore`), para que os dados do seu local não sejam publicados. Sem ele, a compilação para com uma mensagem explicando o que fazer.

---

## O que esperar

### Monitor serial
```
--- Scanner I2C (SDA=D21, SCL=D22) ---
  0x38  AHT20
  0x3C  OLED SSD1306
  0x76  BMP280 (SDO em GND)
--- 3 dispositivo(s) ---
AHT20 : ok (0x38)
BMP280: ok (0x76), chip id 0x58 = BMP280
OLED  : ok (0x3C)
----------------------------------------------
Temperatura (AHT20) :  28.14 °C
Umidade     (AHT20) :   65.4 %
Ponto de orvalho    :   21.0 °C
Sensação térmica    :   30.3 °C
Temperatura (BMP280):  28.05 °C
Pressão no local    :  938.96 hPa
Pressão nível do mar: 1013.37 hPa
Diferença BMP-AHT   :  -0.07 °C
```
Uma nova leitura sai a cada 5 s. O LED azul da placa pisca a cada leitura.

### Display
Três páginas alternam a cada 5 s:

| Página | Conteúdo |
|---|---|
| Temperatura | Temperatura em destaque, umidade e sensação térmica |
| Pressão | Pressão ao nível do mar em destaque e pressão no local |
| Detalhes | Ponto de orvalho, temperatura do BMP280, estado dos sensores e tempo ligado |

---

## Solução de problemas

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Scanner não encontra nada | 3V3/GND/SDA/SCL soltos ou trocados | Confira as ligações e o trilho de alimentação da protoboard |
| BMP280 aparece em `0x77` | SDO ligado em 3V3 ou solto | Ligue SDO em GND, ou troque `ADDR_BMP280` em `include/config.h` |
| BMP280 não aparece | CSB em GND (modo SPI) | Ligue CSB em 3V3 |
| OLED apagado com `0x3C` no scanner | Contraste ou módulo diferente (SH1106) | Confirme que é SSD1306 |
| OLED aparece em `0x3D` | Jumper de endereço do módulo | Troque `ADDR_OLED` em `include/config.h` |
| Temperatura 1–3 °C acima do real | Calor do ESP32 | Afaste os sensores do ESP32 |
| Erro `Falta include/site.h` ao compilar | Configuração do local não criada | `cp include/site.h.example include/site.h` |
| Computador não reconhece a placa | Cabo só de carga ou falta de driver | Troque o cabo; instale o driver CP210x ou CH340 |

---

## Estrutura do repositório

```
GWeather/
├── README.md
├── docs/
│   ├── PLANO.md              Plano e fases
│   └── plano.html            Plano visual com diagrama de ligações
└── firmware/                 Projeto PlatformIO
    ├── platformio.ini
    ├── include/
    │   ├── config.h          Pinos, endereços e intervalos
    │   └── site.h.example    Modelo da configuração do local
    ├── src/
    │   ├── main.cpp
    │   ├── i2c_scan.*        Scanner do barramento I2C
    │   ├── sensors.*         Leitura do AHT20 e do BMP280
    │   ├── weather_math.*    Altitude, pressão ao nível do mar, ponto de orvalho, sensação térmica
    │   └── display.*         Páginas do OLED
    └── docs/
        ├── hardware.md       Pinagem e periféricos
        └── status.md         Andamento do desenvolvimento
```
