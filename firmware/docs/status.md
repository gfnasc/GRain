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

- Fase 12 concluída e validada pelo usuário: configurações só no ESP32; painel só para visualização.
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
  - A localização que estava no banco foi apagada pela primeira cópia; o usuário digitou de novo na página local.
  - Flash: release 84,5 % da partição de app.

- Fase 13 concluída e validada pelo usuário (só nuvem): Outdoor. Aeroporto 24 h a 2 km da estação.
  - `web/server/utils/metar.ts` (aeroporto mais próximo por bbox, METAR em JSON, tradução para português),
    `outdoor.ts` (coleta a cada ingest: METAR a cada 10 min, Open-Meteo a cada 30 min; 72 h de histórico
    ao detectar o aeroporto; inserção em lote), tabela `outdoor_obs`, estado em `devices.outdoor`.
  - Coleta roda depois da resposta (`waitUntil` de `@vercel/functions`); sem isso o primeiro envio levava até 8 s.
  - Aba Agora: Outdoor × Indoor, pressão com tendência e QNH, previsão (`/api/v1/outdoor`, `/api/v1/forecast`).
  - Histórico: `out_temp` (METAR com Open-Meteo nos buracos, p. ex. aeroporto fechado à noite).
  - Testado com localização genérica de teste (apagada depois).

- Fase 14 concluída e validada pelo usuário: pressão calibrada pelo QNH do aeroporto (diferença média 0,04 hPa).
  - Desvio medido antes: estação 2,3 hPa abaixo do QNH (desvio-padrão 0,23 em 16 pares).
  - Firmware 0.10.0: `Reading.seaLevelRawHpa` (sem calibração) vai como `pres_raw`; `calibration.pres_auto`
    (padrão true) e `pres_auto_offset`; com auto ligado, o ajuste manual de pressão é ignorado e o desvio é
    somado ao nível do mar; o desvio chega em `pres_offset` na resposta do ingest e é gravado se mudar ≥ 0,05.
  - Nuvem: `web/server/utils/pressure.ts` (mediana de 7 dias de QNH − pres_raw, pares a ±5 min, mínimo 12);
    coluna `readings.pres_raw`; `out_pres` no histórico; calibração em `latest` e na aba Estação.
  - Histórico corrigido uma vez: 105 médias antigas (sem pres_raw) ganharam +2,29 hPa; original em pres_raw.
  - Aplicado na placa às 10:02 de 13/10: 1014,4 → 1016,7 hPa.

- Fase 15 concluída e validada pelo usuário, dividida em 15a (nuvem e painel) e 15b (ESP32).
  - 15a no ar: `web/server/utils/rain.ts` com M1 (tendência descontada a maré barométrica: onda de 12 h ~1,1 hPa
    + 24 h ~0,6 hPa na hora solar), M2 (METAR: chuva, VCSH/VCTS, CB/TCU, T−Td, teto, rajadas; TAF por janela),
    M3 (Open-Meteo hourly + minutely_15 + CAPE, buscado a cada 30 min em devices.outdoor.rain).
    Tabelas `rain_predictions` (outcome verificado quando a janela termina) e `rain_reports`; `outdoor_obs.precip`.
    Placar (Brier, skill vs. climatologia, acertos/alarmes/perdidas, confiabilidade). Destaque: menor Brier com
    ≥ 100 previsões e ≥ 5 chuvas; antes disso M3. Ingest devolve `rain` compacto para a estação.
    Painel: card na Agora, aba Chuva (métodos, gráfico com trechos de chuva, placar, explicação).
    Primeiras previsões em produção às 10:30 de 13/10: M1 52–79 %, M2 73–94 % (queda real de pressão + TAF
    com TSRA), M3 8–44 %; o placar vai dizer quem acerta.
  - 15b gravado (firmware 0.11.0, release 85,6 %) após reconectar o cabo; previsão chegando na estação
    (/api/v1/rain fresh) e teste de alerta ok (ativo 72 % em 1 h durante o teste).
    Página "Chuva" (PG_RAIN), alerta (`rain_alert`: enabled, threshold, hours, mode) com inversão do SSD1306
    (0xA7/0xA6) ao disparar, guarda-chuva no cabeçalho, tela com gotas animadas (updateDisplayArea nos tiles 0..5)
    a cada duas páginas, histerese de 10 pontos, respeita modo noturno; teste de 12 s. Página local com card de chuva,
    botão "Está chovendo aqui" (repassado com retentativa de 1 min) e opções do alerta.

  - Página local reorganizada em abas (Agora, Display, Ajustes, Sistema; barra embaixo, aba no hash) com
    identidade de aparelho (cabeçalho escuro, selo APP LOCAL · ESP32, tirinha estilo OLED com a página atual,
    fonte mono, ciano) para não confundir com o painel; barra de salvar com aviso de alterações; ~27 KB.
    Testada no navegador com um proxy só leitura para a estação.

- Firmware 0.11.1: troca da senha da estação (8 a 63 caracteres; NVS `portal_pass`): aba Sistema do app local
  (pede a atual), `POST /api/v1/system/password` e console `senha <nova>` / `senha aleatoria`. Senha definida pelo
  usuário. Senha do painel também trocada a pedido (hash na Vercel, texto em web/.panel-password.txt).

- Firmware 0.11.2 + nuvem: a previsão de chuva passou a ser calculada antes da resposta do ingest (antes vinha
  da rodada anterior, ~10 min atrasada na estação); cor de nível só na barra dos mostradores do app local.

- Firmware 0.12.0: médias de 5 min (UPLINK_INTERVAL_S 300, fila 4032); contadores no app local (próximo envio,
  próxima previsão, quadro na aba Sistema). Nuvem: METAR a cada envio, `GET /api/v1/sync`, faixa SyncTimers nas
  abas Agora e Chuva (também é o relógio de atualização da página), cobertura por intervalos de 10 min, pares da
  pressão com as médias dos 10 min anteriores. Previsão de chuva segue a cada 10 min (volume no banco).

- Firmware 0.12.1: os três métodos no app local (tabela no card de chuva); a nuvem manda `rain.all` no ingest.

- Fase 16 concluída e validada pelo usuário (só nuvem): `web/server/utils/indoor.ts` + `GET /api/v1/indoor` + card "No escritório"
  na aba Agora (conforto, janela, ar seco, mofo pelas horas de umidade ≥ 70 % em 72 h, atraso térmico por correlação
  hora a hora Indoor × Outdoor com defasagem de 0 a 8 h, mínimo de 24 pares); resumo por dia com Outdoor.
  METAR das 14:00 de 13/10 registrou -RA, mas o usuário viu que não choveu (nem no aeroporto, visível da janela):
  `rain_reports.raining` (false = "não choveu") desconta METAR de chuva a até 30 min; registrado e as 27 previsões
  afetadas reconferidas como sem chuva (M2/M3 erraram, M1 acertou).

- Fase 17 (em validação): firmware 0.13.x, módulo `health`: watchdog de 60 s (loop e tarefa da nuvem),
  ArduinoOTA porta 3232 com a senha da estação (env `ota`, release, GRAIN_OTA_PASS), autocura (30 min sem Wi-Fi,
  6 h de nuvem falhando), motivo do reinício e contador de boots na NVS, "!" no cabeçalho do OLED.
  A estação passa a rodar o build de release (dev já em 94 % da partição). Nuvem: `status_log`, `GET /api/v1/health`,
  seção Saúde na aba Estação (janela desde a gravação do firmware atual). OTA testado 3 vezes (≈ 50 s cada).
  Depois da reinicialização pela OTA, o mDNS não anuncia o AAAA e o macOS espera 5 s em grain.local; tentativa de
  reanunciar no GOT_IP6 não resolveu e foi desfeita; o usuário acessa pelo IP. Teste de 72 h começou às 15:11 de 13/10.

- Fase 18 (implementada, em teste): firmware 0.14.0 (OTA). App local: tabela com os cinco métodos (M1, M2, M2T, M3,
  M4), botão "Não choveu hoje" (com confirmação; `POST /api/v1/rain/report {"kind":"nao_choveu_hoje"}`), aba Sistema
  com os prazos dos testes (`cloud.tests`, vindo da nuvem a cada envio). Nuvem: M2T e M4 por regressão logística
  com prior nos pesos fixos (`rainlearn.ts`, treino a cada 3 h em `rain_models`), "Não choveu hoje" com
  `rain_reports.covers_from`, testes em `trials` + `utils/trials.ts`, mostradores com contagem regressiva no painel.
  Teste geral antes do início: estação no 0.14.0 sem erros, cinco métodos na estação e no painel, treino rodando,
  "Não choveu hoje" de ponta a ponta (estação → nuvem → painel → desfazer), todos os endpoints do painel 200.

- Firmware 0.14.1 (14/10): o 0.14.0 reiniciava a cada poucas horas com "nuvem sem resposta por 6 h" sem a nuvem ter
  falhado (3 vezes em 24 h). A autocura lia a hora antes do estado da nuvem; se a tarefa da rede gravava lastOkEpoch
  1 s à frente nesse meio-tempo, `now - since` (sem sinal) dava ~4 bilhões. Agora lê a hora depois, confere a cada
  10 s e exige `now > since`. Nenhuma média nem rodada se perdeu nos reinícios (fila).
- Nuvem (14/10): a cobertura do teste de 30 dias não conta rodadas sem METAR (o aeroporto fica horas sem publicar
  de madrugada) nem sem modelo; perdida = rodada sem previsão nenhuma ou com o treinado faltando com os dados.

- Nuvem (15/10): (1) memória estável do teste de 72 h passa a ser medida pela memória livre (o heap_min só desce a
  cada pico passageiro e marcou falha sem vazamento); (2) conferência da chuva: sem METAR na janela, valem os boletins
  vizinhos (anterior e seguinte sem chuva nem RE*, sem buraco > 3 h → não choveu); o modelo só decide com o aeroporto
  calado > 3 h. A regra antiga marcou 42 janelas de chuva falsa na madrugada de 15/10 (céu limpo no aeroporto e
  0,1–0,2 mm estimados pelo Open-Meteo). Últimos 3 dias reconferidos e treino refeito; aprovado pelo usuário, apesar
  de mexer na conferência durante o teste (vale igual para todos os métodos).

- Fase 17 concluída: teste de 72 h (14/10 17:50 a 17/10 17:50, firmware 0.14.1) passou em 17/10 às 17:50: 72 h sem reinícios, sem buracos no histórico (100 % das médias) e memória livre estável (195,7 KB no início, 198,9 KB no fim; pior momento 102 KB).
  Depois do teste, 4 reinícios por falta de energia ("energia ligada"): 18/10 13:26, 22/10 08:40 e 09:21, 25/10 15:36
  (este durante chuva forte); nenhum por watchdog ou autocura. Só 1 buraco de 10 min (25/10 15:25).

## Em andamento
- Teste de chuva (Fase 18): de 13/10 16:25:22 a 12/11 16:25 (hora local); não recomeça com gravações.
- Pendente da fase 7: conferir ~144 médias no primeiro dia completo (resumo por dia).

## Próximos passos
1. Fechar a Fase 18 em 12/11: placar do teste (M2T e M4 contra M1–M3), cobertura ≥ 90 %, pelo menos 5 horas de chuva.
2. Fase 19: WhatsApp (configurado na página local).

## Problemas conhecidos
- M2T e M4 treinam uma regressão por janela (1, 2 e 3 h) separadamente; às vezes a chance de 2 h sai 1 ponto abaixo
  da de 1 h (ex.: M4 28 %, 27 %, 30 % às 17:10 de 13/10). Cosmético. Não mexer durante o teste de 30 dias; depois,
  forçar janelas crescentes (p2 ≥ p1, p3 ≥ p2) na saída.
- Log `[W][Wire.cpp] Bus already started in Master Mode` no boot: as bibliotecas chamam `Wire.begin()` de novo. Inofensivo.
- Abrir a porta serial no macOS reinicia a placa (DTR/RTS).
- Após reinício por OTA, grain.local demora ~5 s no macOS (sem AAAA no mDNS); pelo IP é imediato.
- Se a placa for reconectada (ou passar por hub), o nó /dev/cu.usbserial-0001 pode ficar velho (erro 22 no
  tcsetattr): reconectar o cabo direto no Mac resolve.
- Durante o portal do WiFiManager a porta 80 é dele; o servidor da API só pode subir com o portal fechado.
- Altitude do terreno em `site.h` é estimada; conferir com a altitude da rua e com o QNH do METAR do aeroporto mais próximo.
