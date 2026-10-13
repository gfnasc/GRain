# Status

Memória de continuidade entre sessões. O Claude lê isto ao começar e **atualiza ao terminar** cada tarefa.
Plano geral: `../../docs/PLANO.md` e `../../docs/plano.html`.

## Arquitetura (v5, projeto GRain)
Painel em Nuxt na Vercel (pasta `web/`, projeto `grain`; domínio novo, com o antigo
mantido como alias). O ESP32 mede, mostra no OLED, envia os dados para a nuvem e
mantém uma API local + página mínima em `grain.local`. Plano v5: Outdoor (dados externos) + Indoor
(escritório), chuva em 1–3 h, configurações só no ESP32 (fase 12), painel só leitura, WhatsApp na fase 19.

## Estado atual
- Fases 1–3 concluídas (montagem, sensores, OLED com abertura e páginas).
- Fase 4 concluída e validada na placa:
  - `src/netmgr.*`: WiFiManager não bloqueante; rede de configuração com senha aleatória
    (gerada após ligar o rádio, guardada na NVS em `gweather/portal_pass`) e mostrada no OLED.
  - NTP com fuso de `/config.json` (`time.tz`); mDNS (ping ok a partir do Mac).
  - `src/settings.*`: `/config.json` em LittleFS, criado com valores de `config.h`/`site.h`.
  - `src/console.*`: `ajuda`, `status`, `config`, `set`, `config padrao`, `wifi portal|esquecer`, `reiniciar`.
  - OLED: relógio no cabeçalho, página Rede, tela fixa "Configurar Wi-Fi" com o portal aberto.
  - Serial: uma linha por leitura.
  - `partitions.csv`: app0/app1 de 1,44 MB (OTA) + LittleFS 1 MB + coredump.
  - Teste: após vários reinícios, voltou sozinho ao Wi-Fi, hora sincronizada em ~2 s, config preservada.
- `dev` e `release` compilam (release ~70 % de uma partição de app).

- Fase 5 concluída e validada (celular ok):
  - `src/webapi.*`: WebServer na porta 80, só com Wi-Fi e portal fechado; rotas `/`, `now`, `status`,
    `config` (GET/PUT parcial tudo-ou-nada, exige `Content-Type: application/json`), `system/reboot`.
  - Basic auth `admin` + senha da estação (`portal_pass` na NVS) nas rotas que alteram algo; CORS `*`.
  - `src/webpage.h`: página mínima de 4,6 KB embutida (PROGMEM).
  - IPv6 link-local ligado em STA_CONNECTED para o mDNS responder AAAA (sem espera de 5 s no macOS).
  - Contrato em `../../docs/api-v1.md`.

- Fase 6 concluída e validada (painel no celular pelos dados móveis; queda simulada sem buraco):
  - `web/`: Nuxt 4 na Vercel (projeto `gweather`, deploy automático pelo GitHub com raiz `web/`),
    Neon Postgres (`devices`, `readings`), `POST /api/v1/ingest` com token por estação (sha256),
    login com senha única (nuxt-auth-utils), painel com leitura atual, mín./máx. 24 h e últimas médias.
  - Firmware 0.6.0, `src/uplink.*`: médias de 10 min alinhadas ao relógio (mín. 1/4 do intervalo com dados),
    fila `/queue.jsonl` em LittleFS (até 2016 médias = 14 dias), lotes de até 48, backoff 1→30 min,
    HTTPS com raízes de `src/certs.h` (GTS Root R1, GlobalSign Root CA, ISRG Root X1).
  - URL e token na NVS (`cloud_url`, `cloud_token`); comandos `nuvem ...`; estado em `/api/v1/status`
    (`cloud`) e na página Rede do OLED.
  - Primeiro envio real aceito às 16:10 de 12/10.
  - Flash: release ~83 % da partição de app (TLS adicionou ~165 KB).

- Fase 7 concluída (conferida no celular): `web/app/pages/historico.vue`, `TimeChart.vue` (uPlot),
  `GET /api/v1/readings|daily|readings.csv`; fuso via `NUXT_PUBLIC_TZ`. Gráfico e CSV conferidos com os mesmos pontos.

- Fase 8 concluída e validada pelo usuário:
  - Firmware 0.7.0: `settings` com páginas (`PageCfg`, até 8), modo noturno, calibração e `cloudConfigVersion`
    (schema 2, migra o /config.json antigo); display com páginas Umidade, Relógio, Hoje e Resumo;
    calibração aplicada em `sensors`; mín./máx. do dia (`sensorsToday`).
  - Nuvem: tarefa FreeRTOS no núcleo 0 faz envio e consulta (`/api/v1/device/config` a cada 60 s);
    configuração recebida é aplicada no loop principal (`settingsApplyCloud`).
  - Correção: resposta 204 não pode ter o corpo lido (travava a tarefa).
  - Painel: `/display` com editor e prévia do OLED; ida e volta medida em ~40 s.

- Firmware 0.7.1: favicon na página local; controles do display (`POST /api/v1/display`: página, pausa, tela),
  `POST /api/v1/cloud/sync`, comandos `tela`, `carrossel`, `pagina`; página local com seções Display,
  Nuvem e Manutenção. Painel na nuvem com o mesmo ícone (SVG, ICO e apple-touch-icon).

- Fase 9 (WhatsApp) chegou a funcionar na nuvem, mas foi encerrada na v5 sem validação e removida na fase 11.

- Fase 11 concluída e validada pelo usuário: projeto renomeado para GRain; estação no escritório desde 13/10 07:50.
  - Firmware 0.8.0: hostname `grain` (migra `gweather` ao carregar o /config.json), rede `GRain-Setup`,
    nome na abertura, títulos. Namespace da NVS continua `gweather` (URL, token e senha do portal preservados);
    novos `device_id` usam o prefixo `grain-` (a estação atual mantém o id antigo).
  - Repositório renomeado para `gfnasc/GRain`; projeto da Vercel `grain`; estação apontada para o domínio novo.
  - Nuvem: WhatsApp removido (rotas `notify`, agenda, alertas, aba Mensagens, tabelas `recipients`,
    `notifications` e coluna `devices.notify`); localização copiada para `devices.site`.
  - Tabela `placements` (`sacada`/`escritorio`) + `pnpm placement`: temperatura e umidade da sacada
    hachuradas nos gráficos e fora de extremos/médias (readings, daily, latest); coluna `local` no CSV.
  - Fora do git até a fase 13: `web/server/utils/forecast.ts` e `web/server/api/v1/forecast.get.ts`.

- Fase 12 (em validação): configurações só no ESP32; painel só para visualização.
  - Firmware 0.9.0: `/config.json` schema 3 com `location` (lat, lon, label; NAN = sem localização);
    `settingsRevision()` muda a cada gravação e `uplinkLoop` monta a cópia (display, calibração, localização);
    a cópia vai em todo `POST /api/v1/ingest` e é enviada na hora quando muda (lote vazio permitido);
    saiu a consulta `GET /api/v1/device/config` e `settingsApplyCloud`. Status: `cloud.config_sent`/`config_pending`.
  - Página local (~18 KB): seção Configurações com editor das páginas (tipo, campos do Resumo, ordem),
    tempo, brilho, modo noturno, calibração e localização (colar "lat, lon" do Google Maps na latitude separa sozinho);
    salva com `PUT /api/v1/config` e a senha da estação. Sem prévia do OLED (os botões de página mostram no display).
  - Nuvem: ingest valida e guarda `devices.station_config`/`station_config_at` e `devices.site`; rotas
    `config` e `device/config` removidas; aba Estação somente leitura com prévia (`GET /api/v1/station`); `/display` → `/estacao`.
  - Teste: `set display.page_s 7` no console → cópia no banco ~2 s depois.
  - A localização que estava no banco foi apagada pela primeira cópia (a estação ainda não tinha localização):
    precisa ser digitada na página local.
  - Flash: release 84,5 % da partição de app.

## Em andamento
- Fase 12: validação do usuário (salvar pela página local, conferir a aba Estação, digitar a localização).
- Pendente da fase 7: conferir ~144 médias no primeiro dia completo (resumo por dia).

## Próximos passos
1. Fase 13: Outdoor (METAR do aeroporto mais próximo e Open-Meteo), cards Outdoor × Indoor e previsão na aba Agora.

## Problemas conhecidos
- Log `[W][Wire.cpp] Bus already started in Master Mode` no boot: as bibliotecas chamam `Wire.begin()` de novo. Inofensivo.
- Abrir a porta serial no macOS reinicia a placa (DTR/RTS).
- Durante o portal do WiFiManager a porta 80 é dele; o servidor da API só pode subir com o portal fechado.
- Altitude do terreno em `site.h` é estimada; conferir com a altitude da rua e com o QNH do METAR do aeroporto mais próximo.
