-- Esquema do banco do GRain (Postgres / Neon).
-- Aplicar com: pnpm db:migrate  (idempotente)

-- Estações cadastradas. O token do aparelho nunca é guardado em texto puro.
create table if not exists devices (
  id           text primary key,                 -- device_id do ESP32, ex.: grain-a1b2c3
  name         text not null default 'GRain',
  token_hash   text not null,                    -- sha256 hex do token
  created_at   timestamptz not null default now(),
  last_seen_at timestamptz,
  last_status  jsonb                              -- firmware, rssi, uptime, heap, fila...
);

-- Médias enviadas pelo ESP32: de 5 min (desde o firmware 0.12.0); antes, de 10 min.
create table if not exists readings (
  device_id text        not null references devices(id) on delete cascade,
  ts        timestamptz not null,                 -- início do intervalo da média (UTC)
  temp      real,                                 -- °C (AHT20)
  hum       real,                                 -- %  (AHT20)
  pres_slp  real,                                 -- hPa ao nível do mar (BMP280)
  dew       real,                                 -- °C, ponto de orvalho
  primary key (device_id, ts)
);

create index if not exists readings_ts_idx on readings (ts desc);

-- Fase 12: a configuração (display, calibração e localização) é feita só no ESP32.
-- A estação manda uma cópia junto com as médias; o painel apenas mostra.
-- As colunas da fase 8 (edição pelo painel) saíram.
alter table devices drop column if exists config;
alter table devices drop column if exists config_version;
alter table devices drop column if exists config_updated_at;
alter table devices drop column if exists config_applied_version;
alter table devices drop column if exists last_poll_at;
alter table devices add column if not exists station_config    jsonb;        -- cópia enviada pela estação
alter table devices add column if not exists station_config_at timestamptz;  -- quando a cópia chegou

-- Fase 11 (v5): o WhatsApp da fase 9 foi removido; volta na fase 19 com novo desenho.
drop table if exists notifications;
drop table if exists recipients;
alter table devices drop column if exists notify;

-- Localização da estação (latitude, longitude e nome curto), usada para os dados externos.
-- Fica só no banco, nunca no repositório. Vem da cópia da configuração enviada pela estação.
alter table devices add column if not exists site jsonb;

-- Onde a estação estava em cada período. Na "sacada", temperatura e umidade não
-- representam o ambiente (sol direto e calor do prédio): ficam no histórico, marcadas,
-- e fora das mínimas e máximas. A pressão vale em qualquer local.
-- Registrar mudança: pnpm placement <sacada|escritorio> [quando]
create table if not exists placements (
  id         serial primary key,
  device_id  text        not null references devices(id) on delete cascade,
  place      text        not null check (place in ('sacada', 'escritorio')),
  started_at timestamptz not null,
  ended_at   timestamptz,                         -- null = local atual
  check (ended_at is null or ended_at > started_at)
);
create unique index if not exists placements_current_idx on placements (device_id) where ended_at is null;

-- Fase 13: Outdoor. Observações de fora: METAR do aeroporto mais próximo (de hora em
-- hora, ou antes quando o tempo muda) e condição atual do Open-Meteo (reserva).
-- Coletadas a cada envio da estação (a cada 5 min), sem cron.
create table if not exists outdoor_obs (
  device_id  text        not null references devices(id) on delete cascade,
  source     text        not null check (source in ('metar', 'open-meteo')),
  obs_at     timestamptz not null,               -- horário da observação (UTC)
  station    text,                               -- código do aeroporto (METAR)
  temp       real,                               -- °C
  dew        real,                               -- °C
  hum        real,                               -- %
  pres       real,                               -- hPa: QNH (METAR) ou nível do mar (Open-Meteo)
  wind_dir   smallint,                           -- graus; null = variável ou calmo
  wind_kmh   real,
  gust_kmh   real,
  wx         text,                               -- tempo presente (METAR), ex.: -SHRA VCTS
  clouds     jsonb,                              -- camadas (METAR)
  code       smallint,                           -- código WMO (Open-Meteo)
  raw        text,                               -- METAR original
  fetched_at timestamptz not null default now(),
  primary key (device_id, source, obs_at)
);
create index if not exists outdoor_obs_recent_idx on outdoor_obs (device_id, obs_at desc);

-- Aeroporto detectado para a localização atual e controle da coleta.
alter table devices add column if not exists outdoor jsonb;

-- Fase 14: pressão calibrada pelo aeroporto. A estação manda também a pressão ao nível do
-- mar sem nenhuma calibração (pres_raw); a nuvem calcula o desvio em relação ao QNH do
-- METAR (mediana de 7 dias) com ela, sem realimentação, e devolve na resposta do envio.
alter table readings add column if not exists pres_raw real;  -- hPa, nível do mar sem calibração

-- Fase 15: chuva nas próximas 1 h, 2 h e 3 h.
-- Chuva medida pelo modelo, para verificar quando não há METAR.
alter table outdoor_obs add column if not exists precip real;  -- mm no intervalo anterior (Open-Meteo)

-- Previsões de cada método, feitas a cada 10 min, e o que aconteceu depois (placar).
create table if not exists rain_predictions (
  device_id   text        not null references devices(id) on delete cascade,
  issued_at   timestamptz not null,                -- início do intervalo de 10 min da previsão
  method      text        not null check (method in ('m1', 'm2', 'm3')),
  horizon_h   smallint    not null check (horizon_h between 1 and 3),
  prob        real        not null,                -- 0 a 1
  reason      text,
  features    jsonb,                               -- dados usados no cálculo
  outcome     boolean,                             -- choveu na janela? null = ainda não verificado
  verified_at timestamptz,
  primary key (device_id, issued_at, method, horizon_h)
);
create index if not exists rain_predictions_pending_idx on rain_predictions (device_id, issued_at) where outcome is null;

-- Botão "Está chovendo aqui" (painel ou página local da estação): vale mais que o aeroporto.
create table if not exists rain_reports (
  device_id text        not null references devices(id) on delete cascade,
  ts        timestamptz not null,                  -- início do intervalo de 10 min
  via       text        not null check (via in ('painel', 'estacao')),
  primary key (device_id, ts)
);

-- Observação local negativa ("não choveu"): um METAR de chuva com um "não choveu" a até
-- 30 min dele deixa de contar como chuva (o aeroporto pode registrar poucas gotas ou virga).
alter table rain_reports add column if not exists raining boolean not null default true;

-- Fase 17: saúde da estação (teste de 72 h). Um registro por envio, guardado por 30 dias.
create table if not exists status_log (
  device_id text        not null references devices(id) on delete cascade,
  ts        timestamptz not null default now(),
  fw        text,
  uptime_s  integer,
  heap      integer,
  heap_min  integer,
  rssi      smallint,
  queue     integer,
  boots     integer,
  reset     text,
  primary key (device_id, ts)
);

-- Fase 18: métodos treinados. M2T (estação + aeroporto) e M4 (estação + aeroporto + modelo)
-- aprendem os pesos com as previsões já conferidas; os pesos ficam aqui (refeitos a cada 3 h).
alter table rain_predictions drop constraint if exists rain_predictions_method_check;
alter table rain_predictions add constraint rain_predictions_method_check check (method in ('m1', 'm2', 'm2t', 'm3', 'm4'));
create table if not exists rain_models (
  device_id  text        not null references devices(id) on delete cascade,
  method     text        not null check (method in ('m2t', 'm4')),
  horizon_h  smallint    not null check (horizon_h between 1 and 3),
  weights    jsonb       not null,                 -- peso de cada termo (nome → valor)
  n          integer     not null,                 -- previsões conferidas usadas no treino
  events     integer     not null,                 -- delas, com chuva
  trained_at timestamptz not null default now(),
  primary key (device_id, method, horizon_h)
);

-- Testes longos: o da chuva (30 dias) começa na primeira mensagem do firmware que o inicia e
-- fica guardado aqui (o status_log só guarda 30 dias). O de robustez (72 h) é contado do
-- status_log, desde a gravação do firmware atual.
create table if not exists trials (
  device_id  text        not null references devices(id) on delete cascade,
  key        text        not null,                 -- 'chuva' ou 'robustez' (início oficial marcado à mão)
  started_at timestamptz not null,
  fw         text,
  primary key (device_id, key)
);

-- Botão "Não choveu hoje": um "não choveu" que vale de covers_from (meia-noite local) até o
-- aviso, em vez dos 30 min em volta. Um "Está chovendo aqui" continua valendo mais.
alter table rain_reports add column if not exists covers_from timestamptz;
