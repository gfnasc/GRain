# GRain

Estação de chuva com **ESP32 DevKit v1**, sensores **AHT20** e **BMP280** e display **OLED SSD1306** (antes chamada GWeather). Mede temperatura, umidade e pressão, mostra os dados no display e envia médias de 10 min para um painel em **Nuxt na Vercel**, com histórico e gráficos. O objetivo da versão 5 é prever chuva para 1 h, 2 h e 3 h cruzando o barômetro da estação com dados externos, com dois modos: **Outdoor** (lá fora) e **Indoor** (o ambiente onde a estação fica).

> *ESP32 rain station (AHT20 + BMP280 + SSD1306 OLED). Documentation in Portuguese.*

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
| 3 | Display OLED com páginas rotativas | ✅ Concluída |
| 4 | Wi-Fi, hora pela internet (NTP), mDNS, configuração e console serial | ✅ Concluída |
| 5 | API local `/api/v1` e página mínima no ESP32 | ✅ Concluída |
| 6 | Nuvem: Nuxt na Vercel, banco e envio de dados pelo ESP32 | ✅ Concluída |
| 7 | Histórico, gráficos e exportação CSV no painel | ✅ Concluída |
| 8 | Personalização do display pelo painel | ✅ Concluída |
| 9 | Previsão e WhatsApp | ⏹️ Encerrada na v5 (volta na 19) |
| 10 | Robustez | ➡️ Movida para a 17 |
| 11 | GRain: novo nome, flag "sacada" no histórico, remoção do WhatsApp | ✅ Concluída |
| 12 | Configurações só no ESP32; painel só para visualização | ✅ Concluída |
| 13 | Outdoor: dados externos (METAR e Open-Meteo) | ✅ Concluída |
| 14 | Pressão calibrada pelo aeroporto | ✅ Concluída |
| 15 | Chuva v1: métodos M1 a M3, placar, aba Chuva, página e alertas no OLED | ✅ Concluída |
| 16 | Indoor: conforto, ar seco, mofo, janela, atraso térmico | 🔧 Em validação |
| 17 | Robustez: watchdog, OTA, teste de 72 h | Planejada |
| 18 | Chuva v2: M2 treinado (estação + aeroporto) e M4 combinado, com aprendizado (teste de 30 dias) | Planejada |
| 19 | WhatsApp, configurado no ESP32 | Planejada |

Plano completo: [docs/PLANO.md](docs/PLANO.md). Versão visual, com diagrama interativo de ligações e montagem passo a passo: [docs/plano.html](docs/plano.html) (baixe o arquivo e abra no navegador).

### Arquitetura

```
ESP32 ──envia leituras a cada 10 min (HTTPS)──► Nuxt na Vercel (API + banco)
  │                                                   │
  └─ página mínima em grain.local                     └─ painel completo, com login,
     (funciona sem internet)                             acessível de qualquer lugar
```

O ESP32 mede, mostra no display e envia os dados. O painel fica na nuvem porque um site HTTPS não consegue ler a API HTTP local do ESP32 e porque `grain.local` só existe dentro de casa. A partir da fase 12, todas as configurações passam para a página local do ESP32 e o painel fica só para visualização.

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
| ArduinoJson | 7.4.3 |
| WiFiManager | 2.0.17 |

### Instalação

1. Clone o repositório:
   ```bash
   git clone https://github.com/gfnasc/GRain.git
   cd GRain/firmware
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

### Conectar ao Wi-Fi

Na primeira vez, sem Wi-Fi salvo, o display mostra **"Configurar Wi-Fi"** com o nome de uma rede e uma senha:

1. No celular, conecte na rede **GRain-Setup** usando a senha do display. Ela é aleatória, gerada uma vez por placa.
2. A página de configuração abre sozinha. Se não abrir, acesse `http://192.168.4.1`.
3. Toque em **Configure WiFi**, escolha sua rede (**2,4 GHz**; o ESP32 não conecta em 5 GHz), digite a senha e toque em **Save**.
4. O display volta às páginas normais, com a hora no canto. Na rede local, a estação responde em `grain.local`.

O Wi-Fi fica salvo na placa. Para trocar de rede, use o comando `wifi esquecer` no console.

### Página local e API

Com a estação no Wi-Fi, abra **http://grain.local** (ou o IP da página **Rede** do display) em qualquer aparelho da mesma rede. É o **app local** da estação: roda no próprio ESP32, funciona sem internet e é onde ficam **todas as configurações**; o painel na nuvem só mostra os dados e uma cópia da configuração.

Para não confundir com o painel, o app local tem cara de aparelho: cabeçalho escuro com o selo **APP LOCAL · ESP32** e o endereço da estação, uma tirinha no estilo do OLED com a página que está no display naquele momento, tipografia monoespaçada e acento ciano, e as abas numa barra embaixo (**Agora**, **Display**, **Ajustes** e **Sistema**; a aba fica no endereço, ex.: `grain.local/#display`). Nas abas de configuração, uma barra fixa com a senha e o botão Salvar avisa quando há alterações não salvas. Atualiza a cada 5 s. Conteúdo, por aba:

- **Leitura atual** e estado da estação (Wi-Fi, hora, memória, sensores, firmware).
- **Display:** página anterior/próxima, atalho para cada página, pausar/retomar o carrossel e ligar/desligar a tela. Ao navegar com o carrossel girando, a página fica parada por 60 s. Esses controles valem até reiniciar.
- **Chuva nas próximas horas:** chance em 1, 2 e 3 h (calculada na nuvem e recebida a cada envio), motivo, a tabela dos três métodos (M1, M2 e M3, com o destaque sublinhado), aviso quando o alerta está ativo e o botão **Está chovendo aqui** (a estação repassa para a nuvem).
- **Configurações** (pedem a senha da estação para salvar): páginas do display (até 8, em qualquer ordem, com os campos da página Resumo), tempo por página, brilho, modo noturno, alerta de chuva (limite, janela e forma, com botão **Testar alerta**), calibração dos sensores (com a calibração automática da pressão pelo aeroporto) e localização (latitude, longitude e nome, usados pelos dados de fora). Não há prévia na página: os botões de página da seção Display mostram cada página direto no OLED.
- **Nuvem:** último envio, médias na fila, quando a cópia da configuração chegou ao painel e o botão **Sincronizar agora** (envia a fila e a cópia na hora).
- **Manutenção:** reiniciar a estação (usa a mesma senha).

A mesma porta serve a API `/api/v1`:

| Rota | Descrição |
|---|---|
| `GET /api/v1/now` | Leitura atual |
| `GET /api/v1/status` | Aparelho, Wi-Fi, hora, memória e sensores |
| `GET /api/v1/config` | Configuração |
| `PUT /api/v1/config` 🔒 | Altera campos (JSON parcial, tudo ou nada) |
| `POST /api/v1/system/reboot` 🔒 | Reinicia |
| `POST /api/v1/system/password` 🔒 | Troca a senha da estação: `{"new": "..."}` |
| `GET /api/v1/display` | Estado do display: ligado, pausado, página atual, lista de páginas |
| `POST /api/v1/display` | `{"page":"next"\|"prev"\|n}`, `{"paused":true}`, `{"power":false}`, `{"alert_test":true}` |
| `POST /api/v1/cloud/sync` | Envia agora a fila e a cópia da configuração |
| `GET /api/v1/rain` | Chance de chuva em 1, 2 e 3 h, motivo e estado do alerta |
| `POST /api/v1/rain/report` | Aviso "Está chovendo aqui" (repassado para a nuvem) |

🔒 = HTTP Basic com usuário `admin` e a **senha da estação** (a mesma da rede de configuração; o comando `status` no console mostra). Ela nasce aleatória e pode ser trocada na aba **Sistema** do app local (pede a atual) ou no console (`senha <nova>`, útil se você esquecer a atual, porque exige o cabo). Contrato completo: [docs/api-v1.md](docs/api-v1.md).

```bash
curl http://grain.local/api/v1/now
curl -u admin:SENHA -X PUT -H 'Content-Type: application/json' \
  -d '{"display":{"page_s":8},"location":{"label":"Casa"}}' http://grain.local/api/v1/config
```

### Painel na nuvem (Nuxt na Vercel)

O ESP32 calcula médias de 10 min, guarda numa fila na placa (reserva de até 14 dias sem internet) e envia por HTTPS para o app Nuxt em [`web/`](web), que grava no Postgres (Neon) e mostra o painel com login.

Para montar a sua nuvem do zero (projeto na Vercel, banco, senha do painel e token da estação), siga [web/README.md](web/README.md). Depois, no console serial do ESP32:

```
nuvem url https://SEU-PROJETO.vercel.app
nuvem token <token gerado com pnpm device:token>
nuvem
```

O display mostra o estado do envio na página **Rede** (`ok 16:10`, `erro`, `desligada`).

O painel serve só para ver os dados; nada é configurado por ele. Quatro seções:
- **Agora:** card **Chuva nas próximas horas** (chance em 1, 2 e 3 h, nível, motivo e o botão "Está chovendo aqui"); **Outdoor** (lá fora) e **Indoor** (onde a estação está) lado a lado. O Outdoor vem do METAR do aeroporto mais próximo da localização configurada na estação (temperatura, sensação, umidade, orvalho, vento e condição em português), com a condição atual do Open-Meteo como reserva quando o METAR atrasa ou o aeroporto está fechado. O Indoor traz a última média, mínimas e máximas de 24 h e a diferença em relação a fora. Logo abaixo, o card **No escritório**: conforto (faixas de 20 a 26 °C e de 40 a 60 %), sugestão de **abrir ou fechar a janela** (comparando com o Outdoor e com a chuva prevista), avisos de **ar seco** e de **risco de mofo** (horas com umidade acima de 70 % nos últimos 3 dias) e o **atraso térmico** (quantas horas o ambiente demora para acompanhar a temperatura de fora e quanto da variação chega dentro; precisa de 24 h de dados). Abaixo: pressão do barômetro da estação com tendência de 3 h, interpretação (Zambretti) e QNH do aeroporto para comparar; previsão de hoje e amanhã (Open-Meteo); últimas médias e estado da estação.
- **Chuva:** os três métodos lado a lado para cada janela, com o motivo; gráfico das previsões com os trechos em que choveu de fato; placar de 30 dias por método e janela (erro de Brier, habilidade, acertos, alarmes falsos, perdidas); como funciona.
- **Estação:** a configuração atual, como a estação informou (páginas do OLED com prévia, brilho, modo noturno, calibração e localização), e o estado da estação. A cópia chega a cada envio e logo depois de qualquer mudança feita em `grain.local`.
- **Histórico:** períodos de 24 h, 7 e 30 dias ou datas escolhidas; gráficos de temperatura Indoor × Outdoor (com o ponto de orvalho), umidade e pressão (estação × QNH do aeroporto) (com faixa mínima–máxima nos períodos longos e lacunas visíveis quando faltam dados); extremos com horário; resumo por dia com mínimas e máximas Indoor e Outdoor; exportação em CSV (padrão ou para Excel, com a coluna `local`). O período em que a estação ficou na sacada aparece hachurado nos gráficos de temperatura e umidade e fica fora das mínimas, máximas e médias.

### Dados de fora (Outdoor)

A nuvem busca os dados de fora a cada envio da estação (a cada 10 min), sem cron:

- **METAR** do aeroporto mais próximo da localização, detectado sozinho ([aviationweather.gov](https://aviationweather.gov/data/api/), sem chave). Os aeroportos publicam de hora em hora, ou antes quando o tempo muda.
- **Open-Meteo** (sem chave): condição atual a cada 30 min, usada quando o METAR tem mais de 90 min, e previsão diária.
- Quando a localização muda, o aeroporto é detectado de novo e as últimas 72 h dos dois são buscadas para o histórico já começar preenchido.

Sem localização, o painel mostra só o Indoor. A localização e o aeroporto detectado ficam no banco, nunca no repositório.

### Chuva nas próximas 1, 2 e 3 horas

A cada envio da estação (10 min), a nuvem calcula três métodos, do menos ao mais completo, e guarda cada previsão:

| Método | Usa |
|---|---|
| M1 · Barômetro | Queda da pressão da estação em 1 h e 3 h, descontada a maré barométrica diária (nos trópicos a pressão cai ~3 hPa entre 10h e 16h todo dia, sem relação com chuva), e nível da pressão |
| M2 · Barômetro + aeroporto | M1 + METAR (chuva ou trovoada no aeroporto ou perto dele, nuvens CB/TCU, ar perto de saturar, teto baixo, rajadas) + TAF (previsão do aeroporto) |
| M3 · Modelo numérico | Open-Meteo: probabilidade por hora, chuva prevista em blocos de 15 min e CAPE (energia para trovoada) |

Quando a janela termina, a previsão é conferida: choveu se o METAR registrou chuva (fora "nas proximidades") ou se alguém tocou em **Está chovendo aqui** (no painel ou na página local); sem METAR, vale a chuva medida pelo modelo. Uma observação local de **"não choveu"** desconta um METAR de chuva a até 30 min dela (o aeroporto às vezes registra poucas gotas ou chuva que evapora antes do chão). O placar de 30 dias mostra o erro de cada método; o destaque (card, página local e OLED) é o método de menor erro quando o placar tem dados suficientes (100 previsões e 5 chuvas) e, até lá, o modelo. Os pesos dos métodos são fixos por enquanto; a fase 18 cria um método combinado que aprende com o placar.

### Pressão calibrada pelo aeroporto

A pressão ao nível do mar calculada pela estação depende da altitude configurada; um erro de 8 m dá 1 hPa. Com a opção **Calibrar a pressão pelo aeroporto mais próximo** ligada (padrão, na página local), a nuvem compara a pressão **sem calibração** de cada média com o QNH dos METARs no mesmo horário, tira a mediana dos últimos 7 dias (mínimo de 12 pares) e devolve o desvio na resposta de cada envio. A estação aplica o valor (só grava quando muda) e ele substitui o ajuste manual de pressão. Como o cálculo usa a pressão sem calibração, o ajuste não se realimenta. Isso também resolve a altitude quando a estação muda de andar ou de endereço.

No painel, o gráfico de pressão mostra a estação e o QNH do aeroporto juntos, e as abas Agora e Estação mostram o ajuste em uso.

### Tempos de sincronização

| O quê | Quando |
|---|---|
| Leitura dos sensores (estação) | a cada 5 s |
| Média enviada para a nuvem | a cada 5 min (intervalos :00, :05, :10...), logo que o intervalo fecha; com falha, tenta de novo em 1, 2, 4... até 30 min, e a fila guarda até 14 dias |
| Cópia da configuração para a nuvem | em todo envio e na hora (~2 s) quando algo muda na estação, ao ligar e no "Sincronizar agora" |
| Aviso "Está chovendo aqui" (estação → nuvem) | na hora; se falhar, de novo a cada 1 min |
| Previsão de chuva | a cada 10 min (a cada dois envios), antes da resposta: estação e painel recebem a mesma rodada |
| Ajuste da pressão (nuvem → estação) | volta em todo envio; a estação só grava se mudar 0,05 hPa ou mais |
| METAR do aeroporto | conferido a cada envio (5 min); o aeroporto publica de hora em hora ou antes, se o tempo mudar |
| Open-Meteo (condição atual, próximas horas) e TAF | a cada 30 min |
| Previsão de hoje e amanhã (Open-Meteo) | guardada por 30 min |
| Conferência das previsões de chuva | a cada envio, para as janelas que terminaram há mais de 20 min |
| Placar | recalculado a cada 10 min |
| App local (`grain.local`) | atualiza a cada 5 s |
| Painel, aba Agora | atualiza a cada 1 min (previsão do dia a cada 15 min) |
| Painel, aba Chuva | atualiza a cada 2 min |
| "Online" no painel | último envio há menos de 15 min |

Os valores da estação no app local são instantâneos (5 s); os do painel são médias de 5 min, então podem diferir um pouco. Os gráficos de até 2 dias agrupam em 10 min.

**Contadores:** o painel mostra, nas abas Agora e Chuva, quanto falta para a próxima atualização da página, a próxima média da estação, a próxima previsão de chuva, o próximo METAR e o próximo Open-Meteo/TAF. O app local mostra a contagem do próximo envio na tirinha do topo, a próxima previsão no card de chuva e o quadro **Próximas atualizações** na aba Sistema.

### Local da estação (sacada ou escritório)

Na sacada, sol direto e calor do prédio distorcem a temperatura e a umidade; a pressão vale em qualquer lugar. O banco guarda onde a estação estava em cada período (tabela `placements`), e o painel usa isso para marcar o histórico. Para registrar uma mudança, dentro de `web/`:

```bash
pnpm placement escritorio 2026-10-13T08:10-03:00   # a partir desse horário (sem horário = agora)
pnpm placement                                     # lista os períodos
```

### Console serial

Com `pio device monitor` aberto, digite `ajuda`:

| Comando | O que faz |
|---|---|
| `status` | Rede, IP, hora, altitude, sensores, memória e senha da estação |
| `config` | Mostra a configuração gravada (`/config.json`) |
| `set <chave> <valor>` | Altera e salva, ex.: `set site.floor 10`, `set display.page_s 8`, `set location.label Casa`, `set time.tz <-03>3` |
| `config padrao` | Volta aos valores iniciais de `site.h` |
| `nuvem` | Estado do envio: endereço, fila, último envio e último erro |
| `nuvem url <https://...>` / `nuvem token <token>` | Configura o envio para a nuvem (guardado na NVS) |
| `nuvem enviar` / `nuvem limpar` / `nuvem desligar` | Envia a fila e a cópia da configuração agora / descarta a fila / apaga endereço e token |
| `tela ligar` / `tela desligar` | Liga ou desliga o display (até reiniciar) |
| `carrossel pausar` / `carrossel retomar` | Pausa ou retoma a troca de páginas |
| `pagina proxima` / `pagina anterior` / `pagina <n>` | Navega entre as páginas do display |
| `wifi portal` | Abre a rede de configuração sem esquecer o Wi-Fi atual |
| `wifi esquecer` | Apaga o Wi-Fi salvo e reinicia |
| `senha <nova>` / `senha aleatoria` | Troca a senha da estação (8 a 63 caracteres) ou gera uma aleatória |
| `reiniciar` | Reinicia a placa |

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

`site.h` também define o fuso horário (`SITE_TZ`, formato POSIX; Brasília é `"<-03>3"`).

Esses valores são só os **iniciais**: no primeiro boot eles são gravados em `/config.json` na placa e, depois disso, a configuração gravada prevalece. Para mudar, use `set` no console (sem recompilar) ou `config padrao` para voltar aos valores de `site.h`.

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
[net] conectado em "MinhaRede", IP 192.168.1.105
[net] mDNS: grain.local
[net] hora sincronizada: 12/10/2026 15:23:12
Digite "ajuda" para ver os comandos.
[15:23:16] 28.16 °C  65.4 %  orvalho 21.1 °C  sensação 30.4 °C  |  1010.82 hPa (local 936.60)  BMP 27.82 °C
```
Uma linha por leitura, a cada 5 s. O LED azul da placa pisca a cada leitura.

### Display
No boot aparece a tela de abertura com o nome GRain e um spinner animado por 2,5 s. Depois, as páginas alternam no tempo configurado. As páginas, a ordem e o tempo de cada uma são escolhidos na página local (`grain.local`, seção **Configurações**). Tipos disponíveis:

| Página | Conteúdo |
|---|---|
| Temperatura | Temperatura em destaque, umidade e sensação térmica |
| Pressão | Pressão ao nível do mar em destaque e pressão no local |
| Detalhes | Ponto de orvalho, temperatura do BMP280, estado dos sensores e tempo ligado |
| Rede | Nome do Wi-Fi, IP, estado do envio para a nuvem e data/hora |
| Umidade | Umidade em destaque e ponto de orvalho |
| Relógio | Hora grande, dia da semana e data, temperatura e umidade |
| Hoje | Mínima e máxima do dia (com horário), faixa de umidade e amplitude |
| Resumo | Até 4 campos à escolha (temperatura, umidade, pressão, orvalho, sensação, temperatura do BMP280, pressão no local) |
| Chuva | Chance de chuva em 1, 2 e 3 h em barras, com a linha tracejada do limite do alerta; porcentagens acima do limite aparecem invertidas |

Padrão: Temperatura, Pressão, Detalhes e Rede, 5 s cada. A hora aparece no cabeçalho quando cabe. No modo noturno, o display apaga ou escurece no horário escolhido.

**Alerta de chuva** (configurável na página local): quando a chance numa das próximas 1, 2 ou 3 h passa do limite, o display pisca em inverso, um guarda-chuva aparece no cabeçalho de todas as páginas e uma tela "CHUVA provável" com gotas animadas entra no carrossel a cada duas páginas. O alerta só some 10 pontos abaixo do limite, para não ficar piscando, e não acende o display no modo noturno.

Enquanto a rede de configuração estiver aberta, o display fica fixo nas instruções de conexão.

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
| Não aparece a rede GRain-Setup | Já existe Wi-Fi salvo | Use `wifi portal` no console |
| Não conecta ao Wi-Fi | Rede de 5 GHz ou senha errada | Use uma rede de 2,4 GHz; `wifi esquecer` e configure de novo |
| `grain.local` não abre | Aparelho sem suporte a mDNS (alguns Android) ou em outra rede | Use o IP mostrado na página Rede do display |
| `PUT` responde 415 | Falta o cabeçalho JSON | Envie `Content-Type: application/json` |
| Página Rede mostra `erro` na nuvem | Token errado, URL errada ou internet fora | Comando `nuvem` mostra o último erro; a fila guarda as médias e reenvia depois |
| Computador não reconhece a placa | Cabo só de carga ou falta de driver | Troque o cabo; instale o driver CP210x ou CH340 |

---

## Estrutura do repositório

```
GRain/
├── README.md
├── LICENSE
├── web/                      Painel Nuxt na Vercel (ver web/README.md)
├── docs/
│   ├── api-v1.md             Contrato da API
│   ├── PLANO.md              Plano e fases
│   └── plano.html            Plano visual com diagrama de ligações
└── firmware/                 Projeto PlatformIO
    ├── platformio.ini
    ├── partitions.csv        2 apps de 1,44 MB (OTA) + 1 MB de LittleFS
    ├── include/
    │   ├── config.h          Pinos, endereços, intervalos e rede
    │   └── site.h.example    Modelo da configuração do local
    ├── src/
    │   ├── main.cpp
    │   ├── settings.*        Configuração em /config.json
    │   ├── netmgr.*          Wi-Fi, NTP e mDNS
    │   ├── console.*         Comandos pelo serial
    │   ├── i2c_scan.*        Scanner do barramento I2C
    │   ├── sensors.*         Leitura do AHT20 e do BMP280
    │   ├── weather_math.*    Altitude, pressão ao nível do mar, ponto de orvalho, sensação térmica
    │   ├── display.*         Páginas do OLED
    │   ├── webapi.*          Servidor HTTP e API /api/v1
    │   ├── uplink.*          Médias de 10 min, fila e envio para a nuvem
    │   ├── certs.h           Certificados raiz para HTTPS
    │   └── webpage.h         Página mínima (HTML embutido)
    └── docs/
        ├── hardware.md       Pinagem e periféricos
        └── status.md         Andamento do desenvolvimento
```

---

## Licença

[MIT](LICENSE). Você pode usar, modificar e redistribuir, mantendo o aviso de copyright. O software é fornecido sem garantia.
