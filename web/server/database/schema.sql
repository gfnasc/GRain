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

-- Fase 8: configuração do display e calibração, editada no painel.
alter table devices add column if not exists config                 jsonb;
alter table devices add column if not exists config_version         integer not null default 0;
alter table devices add column if not exists config_updated_at      timestamptz;
alter table devices add column if not exists config_applied_version integer;     -- informado pela estação a cada consulta
alter table devices add column if not exists last_poll_at           timestamptz; -- última consulta de configuração

-- Fase 11 (v5): o WhatsApp da fase 9 foi removido; volta na fase 19 com novo desenho.
drop table if exists notifications;
drop table if exists recipients;
alter table devices drop column if exists notify;

-- Localização da estação (latitude, longitude e nome curto), usada para os dados externos.
-- Fica só no banco, nunca no repositório. A partir da fase 12, enviada pela própria estação.
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
