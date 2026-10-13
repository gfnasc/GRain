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

-- Médias de 10 minutos enviadas pelo ESP32.
create table if not exists readings (
  device_id text        not null references devices(id) on delete cascade,
  ts        timestamptz not null,                 -- início do intervalo de 10 min (UTC)
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
-- Coletadas a cada envio da estação (a cada 10 min), sem cron.
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
