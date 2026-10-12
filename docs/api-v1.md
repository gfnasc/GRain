# API v1 — GWeather

Contrato da API HTTP. As rotas do **ESP32** existem desde a fase 5; as rotas da **nuvem** (Nuxt na Vercel) chegam na fase 6 e usam os mesmos nomes de campos.

Convenções:
- JSON em UTF-8. Horários em **UTC epoch (segundos)**; o fuso só é aplicado na exibição.
- Números com unidades fixas: temperatura em °C, umidade em %, pressão em hPa, altitude em m.
- Valor indisponível (sensor com falha, hora não sincronizada) vem como `null`.
- Erros: `{"error": "<código>", "detail": "<explicação>"}` com o status HTTP correspondente.

---

## ESP32 (rede local)

Base: `http://gweather.local` (ou o IP mostrado na página **Rede** do display).

O servidor só responde com Wi-Fi conectado e a rede de configuração fechada. Rotas `GET` são abertas na rede local; rotas que alteram algo exigem **HTTP Basic** com usuário `admin` e a **senha da estação** (a mesma da rede de configuração, exibida pelo comando `status` no console serial).

CORS: `Access-Control-Allow-Origin: *`, para permitir desenvolvimento do painel a partir de `localhost`.

### `GET /`
Página mínima (HTML, sem dependências externas) com leitura atual e estado. Atualiza a cada 5 s.

### `GET /api/v1/now`
Última leitura (feita a cada 5 s).

```json
{
  "device_id": "gweather-a1b2c3",
  "ts": 1791830406,
  "age_s": 0,
  "temp": 28.02,
  "hum": 65.9,
  "dew": 21.1,
  "heat_index": 30.2,
  "temp_bmp": 27.98,
  "pres_station": 936.54,
  "pres_slp": 1010.76,
  "altitude_m": 630.0
}
```

| Campo | Descrição |
|---|---|
| `ts` | Hora da resposta (UTC epoch); `null` antes de sincronizar com NTP |
| `age_s` | Segundos desde a última leitura |
| `temp`, `hum` | AHT20 (fonte principal) |
| `dew`, `heat_index` | Ponto de orvalho e sensação térmica, calculados |
| `temp_bmp` | Temperatura do BMP280 (conferência) |
| `pres_station` | Pressão medida no local |
| `pres_slp` | Pressão reduzida ao nível do mar |
| `altitude_m` | Altitude usada na correção |

Status: `200`; `503` se ainda não houve nenhuma leitura.

### `GET /api/v1/status`

```json
{
  "device": { "id": "gweather-a1b2c3", "firmware": "0.5.0", "uptime_s": 24,
              "heap_free": 226656, "heap_min": 222612, "fs_used": 8192, "fs_total": 1048576 },
  "wifi": { "connected": true, "ssid": "MinhaRede", "ip": "192.168.1.105", "rssi": -45,
            "hostname": "gweather.local" },
  "time": { "synced": true, "tz": "<-03>3", "local": "2026-10-12T15:33:51", "epoch": 1791830031 },
  "sensors": { "aht20": true, "bmp280": true }
}
```

### `GET /api/v1/config`
Configuração gravada em `/config.json`.

```json
{
  "schema": 1,
  "device_id": "gweather-a1b2c3",
  "hostname": "gweather",
  "time": { "tz": "<-03>3", "ntp1": "pool.ntp.org", "ntp2": "time.google.com" },
  "site": { "ground_alt_m": 600, "floor": 10, "floor_height_m": 2.9, "sensor_height_m": 1 },
  "display": { "page_s": 5, "contrast": 128 }
}
```

### `PUT /api/v1/config` 🔒
Altera um ou mais campos. O corpo é um JSON **parcial** no mesmo formato do `GET`. É tudo ou nada: se um campo for inválido, nada é alterado. Exige `Content-Type: application/json`.

```bash
curl -u admin:SENHA -X PUT -H 'Content-Type: application/json' \
  -d '{"display":{"page_s":8},"site":{"floor":10}}' \
  http://gweather.local/api/v1/config
```

| Campo | Regra |
|---|---|
| `hostname` | 1–31 caracteres, letras, números e hífen (aplica após reiniciar) |
| `time.tz` | Fuso POSIX, ex.: `<-03>3` (aplica após reiniciar) |
| `time.ntp1`, `time.ntp2` | Servidores NTP (aplica após reiniciar) |
| `site.ground_alt_m` | −500 a 6000 |
| `site.floor` | 0 a 200 |
| `site.floor_height_m` | 2 a 6 |
| `site.sensor_height_m` | 0 a 50 |
| `display.page_s` | 2 a 60 |
| `display.contrast` | 0 a 255 |

`schema` e `device_id` são somente leitura e ignorados.

Respostas: `200` com a configuração nova · `400 json_invalido` · `401` sem senha ou senha errada · `415 tipo_invalido` sem `Content-Type: application/json` · `422 valor_invalido` com o campo e a regra no `detail`.

### `POST /api/v1/system/reboot` 🔒
Responde `202 {"ok": true}` e reinicia a placa meio segundo depois.

### `GET /api/v1/display`
```json
{ "on": true, "paused": false, "night": false, "page": 1, "hold_s": 0,
  "pages": ["clock", "temp", "fields", "network"], "page_s": 7 }
```
`hold_s`: segundos até o carrossel voltar a girar depois de uma navegação manual.

### `POST /api/v1/display`
Controles temporários (valem até reiniciar), sem senha. Corpo JSON com um ou mais campos:

| Campo | Efeito |
|---|---|
| `"page": "next"` / `"prev"` / `<índice>` | Navega; com o carrossel girando, segura a página por 60 s |
| `"paused": true/false` | Pausa ou retoma o carrossel (o display mostra ‖ no cabeçalho) |
| `"power": true/false` | Liga ou desliga a tela (desligada manualmente vale mais que o modo noturno) |

Responde o estado atualizado (mesmo formato do `GET`). `422` para página fora da lista.

### `POST /api/v1/cloud/sync`
Envia a fila de médias e consulta a configuração da nuvem imediatamente. `202`; `409` se a nuvem não estiver configurada.

### `GET /favicon.svg`
Ícone da estação (o mesmo do painel).

---

## Nuvem (Nuxt na Vercel)

Base: o domínio do projeto na Vercel. Código em [`web/`](../web).

### `POST /api/v1/ingest` 🔑
Recebe um lote de médias de 10 min do ESP32. Autenticação: `Authorization: Bearer <token da estação>` (o banco guarda só o sha256 do token).

```json
{
  "readings": [
    { "ts": 1791831600, "temp": 27.97, "hum": 65.8, "pres_slp": 1010.55, "dew": 21.0 }
  ],
  "status": { "fw": "0.6.0", "rssi": -57, "uptime_s": 563, "heap": 220132, "queue": 0 }
}
```

| Campo | Regra |
|---|---|
| `ts` | Início do intervalo de 10 min, UTC epoch inteiro, entre 2024-01-01 e agora + 5 min |
| `temp` | −40 a 85, ou ausente/`null` |
| `hum` | 0 a 100, ou ausente/`null` |
| `pres_slp` | 800 a 1100, ou ausente/`null` |
| `dew` | −60 a 60, ou ausente/`null` |
| `readings` | Até 500 por lote |
| `status` | Opcional; guardado como o último estado da estação |

Horários já recebidos são ignorados, então o ESP32 pode reenviar com segurança.

Respostas: `200 {"accepted": 1, "duplicates": 0}` · `400` lista ausente · `401` token ausente ou inválido · `413` lote grande demais · `422` valor fora da faixa (o `message` diz qual leitura).

### `GET /api/v1/latest` 🔐
Exige a sessão do painel. Devolve a estação, a média mais recente, as 12 últimas médias e mínimas/máximas de 24 h.

### `POST /api/auth/login`
`{"password": "..."}` → `200` e cookie de sessão (30 dias) · `401` senha incorreta.

### Período (parâmetros comuns às rotas de histórico)

- `?range=24h | 7d | 30d` — relativo a agora (padrão `24h`), ou
- `?from=AAAA-MM-DD&to=AAAA-MM-DD` — dias inteiros no fuso do painel (`NUXT_PUBLIC_TZ`), até 400 dias.

Período inválido responde `400` com a explicação em `message`.

### `GET /api/v1/readings` 🔐
Série agregada para os gráficos. A resolução depende do tamanho do período: até 2 dias, médias de 10 min (como chegam); até 10 dias, 1 h; até 45 dias, 3 h; acima disso, 1 dia. Os intervalos são alinhados ao fuso do painel.

```json
{
  "from": "2026-10-11T19:40:00.000Z", "to": "2026-10-12T19:40:00.000Z", "step_s": 600,
  "points": [
    { "ts": 1791831600, "temp": 27.97, "temp_min": 27.97, "temp_max": 27.97,
      "hum": 65.8, "hum_min": 65.8, "hum_max": 65.8,
      "pres": 1010.55, "pres_min": 1010.55, "pres_max": 1010.55, "dew": 21.0, "n": 1 }
  ],
  "summary": {
    "n": 4, "expected": 144,
    "temp": { "min": { "v": 27.97, "ts": 1791831600 }, "max": { "v": 28.09, "ts": 1791832800 }, "avg": 28.03 },
    "hum":  { "min": { "...": "..." }, "max": { "...": "..." }, "avg": 66.25 },
    "pres": { "min": { "...": "..." }, "max": { "...": "..." }, "avg": 1010.57 }
  }
}
```

`n` em cada ponto é a quantidade de médias de 10 min agregadas. Os extremos do `summary` são exatos (não médias), com o horário em que ocorreram; `expected` é quantas médias de 10 min cabem no período.

### `GET /api/v1/daily` 🔐
Resumo por dia no fuso do painel: mínima, máxima e média de temperatura, umidade e pressão, e a quantidade de médias recebidas (`n`, de 144 possíveis).

### `GET /api/v1/readings.csv` 🔐
Médias de 10 min do período em CSV (download). Colunas: `inicio_utc`, `inicio_local`, `temperatura_c`, `umidade_pct`, `pressao_nivel_mar_hpa`, `ponto_orvalho_c`. Com `&excel=1`: separador `;`, vírgula decimal e BOM UTF-8 para abrir direto no Excel em português.

### Configuração do display e da calibração

Formato (validado no painel, na nuvem e no ESP32; referência em `web/shared/utils/station-config.ts`):

```json
{
  "display": {
    "page_s": 5,
    "contrast": 128,
    "night": { "enabled": false, "from": "23:00", "to": "06:00", "mode": "dim" },
    "pages": [
      { "type": "clock" }, { "type": "temp" },
      { "type": "fields", "fields": ["temp", "hum", "pres", "dew"] }
    ]
  },
  "calibration": { "temp": 0, "hum": 0, "pres": 0 }
}
```

| Campo | Regra |
|---|---|
| `pages` | 1 a 8; `type`: `temp`, `hum`, `pres`, `clock`, `today`, `fields`, `details`, `network` |
| `fields` | Só na página `fields`: 1 a 4 de `temp`, `hum`, `pres`, `dew`, `heat_index`, `temp_bmp`, `pres_station` |
| `page_s` | 2 a 60 (inteiro) |
| `contrast` | 0 a 255 (inteiro) |
| `night` | `enabled` booleano; `from`/`to` em `HH:MM` (podem cruzar a meia-noite); `mode` `off` (apagar) ou `dim` (escurecer) |
| `calibration` | Somado às leituras: `temp` −5 a 5 °C, `hum` −15 a 15 %, `pres` −15 a 15 hPa |

#### `GET /api/v1/config` 🔐
Configuração atual (ou a padrão, com `is_default: true`), `version`, `applied_version` (informada pela estação) e `last_poll_at`.

#### `PUT /api/v1/config` 🔐
Salva a configuração completa e gera a próxima `version`. `422` com a regra violada em `message`.

#### Notificações 🔐

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/api/v1/notify` | Agenda, alertas e local; destinatários (sem a chave); últimos 20 envios |
| `PUT` | `/api/v1/notify` | Salva agenda, alertas e local (validação em `web/shared/utils/notify-config.ts`) |
| `POST` | `/api/v1/notify/recipients` | `{name, phone, apikey}`; telefone no formato `+5511999999999` |
| `PATCH` | `/api/v1/notify/recipients/:id` | `{name?, phone?, apikey?, enabled?}` |
| `DELETE` | `/api/v1/notify/recipients/:id` | Remove |
| `GET` | `/api/v1/notify/preview?type=` | Monta a mensagem (`forecast`, `current`, `trend`, `summary`) sem enviar |
| `POST` | `/api/v1/notify/test` | `{type, recipient_id?}`: envia agora e registra como teste |

A chave do CallMeBot nunca é devolvida ao navegador. Cada envio grava um `slot` único (`agenda:<id>:<data>`, `alerta:<tipo>:<ts>`, `teste:<ts>`), o que impede duplicatas.

#### `GET /api/v1/device/config?v=<versão aplicada>` 🔑
Consultado pelo ESP32 a cada minuto com o token da estação. `204` se a estação já está na versão atual; senão `200 {"version": 3, "config": {...}}`. Cada consulta registra `applied_version` e `last_poll_at`, que o painel usa para confirmar a aplicação e o estado Online. A mesma consulta dispara a agenda de mensagens; o `POST /api/v1/ingest` dispara a avaliação dos alertas.
