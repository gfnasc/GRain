-- Esquema do banco do GWeather (Postgres / Neon).
-- Aplicar com: pnpm db:migrate  (idempotente)

-- Estações cadastradas. O token do aparelho nunca é guardado em texto puro.
create table if not exists devices (
  id           text primary key,                 -- device_id do ESP32, ex.: gweather-a1b2c3
  name         text not null default 'GWeather',
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

-- Fase 9: notificações por WhatsApp (CallMeBot).
alter table devices add column if not exists notify jsonb;  -- agenda, alertas e local da previsão

create table if not exists recipients (
  id         serial primary key,
  name       text not null,
  phone      text not null,           -- formato internacional, ex.: +5511999999999
  apikey     text not null,           -- chave do CallMeBot (nunca devolvida ao navegador)
  enabled    boolean not null default true,
  created_at timestamptz not null default now()
);

-- Registro de envios. "slot" identifica o envio (agenda + dia, alerta + janela):
-- a unicidade garante que nada é enviado duas vezes.
create table if not exists notifications (
  id         serial primary key,
  slot       text not null unique,
  kind       text not null,           -- schedule | alert | test
  type       text,                    -- forecast | current | trend | summary | temp_max | ...
  status     text not null default 'pendente',  -- enviado | parcial | falhou | sem_destinatarios
  detail     text,
  message    text,
  created_at timestamptz not null default now()
);
create index if not exists notifications_created_idx on notifications (created_at desc);
