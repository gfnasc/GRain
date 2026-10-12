# Status

Memória de continuidade entre sessões. O Claude lê isto ao começar e **atualiza ao terminar** cada tarefa.
Plano geral: `../../docs/PLANO.md` e `../../docs/plano.html`.

## Arquitetura (v4)
Painel em Nuxt na Vercel (pasta `web/`, fase 6). O ESP32 mede, mostra no OLED, agenda mensagens,
envia os dados para a nuvem e mantém uma API local + página mínima em `gweather.local`.

## Estado atual
- Fases 1–3 concluídas (montagem, sensores, OLED com abertura e páginas).
- Fase 4 concluída e validada na placa:
  - `src/netmgr.*`: WiFiManager não bloqueante; rede "GWeather-Setup" com senha aleatória
    (gerada após ligar o rádio, guardada na NVS em `gweather/portal_pass`) e mostrada no OLED.
  - NTP com fuso de `/config.json` (`time.tz`); mDNS `gweather.local` (ping ok a partir do Mac).
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

- Fase 8 implementada (em validação):
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

## Em andamento
- Fase 8: fechar documentação (usuário validou).
- Pendente da fase 7: conferir ~144 médias no primeiro dia completo (resumo por dia).

## Próximos passos
1. Fase 9: previsão (Zambretti + Open-Meteo) e WhatsApp (CallMeBot).

## Problemas conhecidos
- O envio HTTPS bloqueia o loop por ~1–3 s a cada 10 min (display pausa um instante).
- Log `[W][Wire.cpp] Bus already started in Master Mode` no boot: as bibliotecas chamam `Wire.begin()` de novo. Inofensivo.
- Abrir a porta serial no macOS reinicia a placa (DTR/RTS).
- Durante o portal do WiFiManager a porta 80 é dele; o servidor da API só pode subir com o portal fechado.
- Altitude do terreno em `site.h` é estimada; conferir com a altitude da rua e com o QNH do METAR do aeroporto mais próximo.
