<script setup lang="ts">
import type { RainResp } from '~/components/RainNow.vue'
// Aba Agora: Outdoor (lá fora, dados externos) × Indoor (escritório, sensores da estação),
// pressão com tendência (barômetro da estação) e previsão de hoje e amanhã.
useHead({ title: 'GRain' })

type Reading = { ts: number; temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null }
type Latest = {
  device: { id: string; name: string; last_seen_at: string | null; status: Record<string, any> | null; place: 'sacada' | 'escritorio' | null; pres_cal: { auto: boolean; offset: number } | null } | null
  latest: Reading | null
  recent: Reading[]
  last24h: { temp_min: number | null; temp_max: number | null; hum_min: number | null; hum_max: number | null; n: number } | null
}
type OutNow = {
  source: 'metar' | 'open-meteo'
  obs_at: string
  temp: number | null
  dew: number | null
  hum: number | null
  feels: number | null
  pres: number | null
  wind_dir: number | null
  wind_kmh: number | null
  gust_kmh: number | null
  condition: string
  raw: string | null
}
type Outdoor = {
  located: boolean
  station: { icao: string; name: string; dist_km: number } | null
  now: OutNow | null
  metar: OutNow | null
  model: OutNow | null
}
type Day = { tmax: number; tmin: number; rain: number | null; text: string }
type Forecast = {
  location: { configured: boolean; label: string }
  today: Day | null
  tomorrow: Day | null
  pressure: { value: number | null; delta_3h: number | null; trend: 'subindo' | 'estável' | 'caindo' | null; outlook: string | null }
}

const { data, refresh, error } = await useFetch<Latest>('/api/v1/latest')
const { data: outdoor, refresh: refreshOut } = await useFetch<Outdoor>('/api/v1/outdoor')
const { data: forecast, refresh: refreshFc } = await useFetch<Forecast>('/api/v1/forecast')
const { data: rain, refresh: refreshRain } = await useFetch<RainResp>('/api/v1/rain')

// Atualiza a cada minuto; o ESP32 envia médias de 10 min e a coleta de fora acompanha.
let timer: ReturnType<typeof setInterval> | undefined
const now = ref(Date.now())
onMounted(() => {
  timer = setInterval(() => {
    now.value = Date.now()
    refresh()
    refreshOut()
    refreshRain()
  }, 60_000)
})
onBeforeUnmount(() => clearInterval(timer))
let fcTimer: ReturnType<typeof setInterval> | undefined
onMounted(() => (fcTimer = setInterval(refreshFc, 15 * 60_000)))
onBeforeUnmount(() => clearInterval(fcTimer))

const fmt = (v: number | null | undefined, d = 1) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d, useGrouping: false })
const tz = useRuntimeConfig().public.tz
const hhmm = (ts: number) => new Date(ts * 1000).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: tz })
const hhmmIso = (iso: string) => hhmm(new Date(iso).getTime() / 1000)

function ago(iso: string | null | undefined) {
  if (!iso) return 'nunca'
  const min = Math.round((now.value - new Date(iso).getTime()) / 60000)
  if (min < 1) return 'agora há pouco'
  if (min < 60) return `há ${min} min`
  const h = Math.floor(min / 60)
  return h < 48 ? `há ${h} h` : `há ${Math.floor(h / 24)} dias`
}
function uptime(s?: number) {
  if (!s) return '–'
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60)
  return `${d ? d + 'd ' : ''}${h}h ${String(m).padStart(2, '0')}m`
}
const DIRS = ['N', 'NE', 'L', 'SE', 'S', 'SO', 'O', 'NO']
const windText = (o: OutNow) => {
  if (o.wind_kmh == null) return '–'
  if (o.wind_kmh < 2) return 'calmo'
  const dir = o.wind_dir == null ? 'variável' : DIRS[Math.round(o.wind_dir / 45) % 8]
  return `${dir} ${fmt(o.wind_kmh, 0)} km/h${o.gust_kmh ? `, rajadas ${fmt(o.gust_kmh, 0)}` : ''}`
}
const out = computed(() => outdoor.value?.now ?? null)
const sourceText = computed(() => {
  const o = out.value
  if (!o) return ''
  if (o.source === 'metar' && outdoor.value?.station) {
    const s = outdoor.value.station
    return `Aeroporto ${s.icao} (${s.dist_km} km) · observação das ${hhmmIso(o.obs_at)}`
  }
  return `Modelo Open-Meteo · ${hhmmIso(o.obs_at)}${outdoor.value?.metar ? ' (METAR atrasado)' : ''}`
})
const trendArrow = { subindo: '↗', 'estável': '→', caindo: '↘' } as const
const PLACE_LABEL = { sacada: 'Sacada', escritorio: 'Escritório' } as const
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="data?.device?.last_seen_at ?? null" />

    <p v-if="error" class="notice crit">Não foi possível carregar os dados. Tente recarregar a página.</p>

    <section v-else-if="!data?.device" class="notice">
      <strong>Nenhuma estação cadastrada ainda.</strong>
      <span>Cadastre com <code>pnpm device:token &lt;device_id&gt;</code> e configure o token no ESP32 com o comando <code>nuvem token</code>.</span>
    </section>

    <template v-else>
      <RainNow :data="rain ?? null" @reported="refreshRain" />

      <section class="modes">
        <article class="mode">
          <header><span class="tag">Outdoor</span><span class="muted small">lá fora</span></header>
          <template v-if="out">
            <span class="v">{{ fmt(out.temp, out.source === 'metar' ? 0 : 1) }}<small>°C</small></span>
            <span class="cond">{{ out.condition || '–' }}</span>
            <dl class="kv">
              <dt>Sensação</dt><dd>{{ fmt(out.feels, 0) }} °C</dd>
              <dt>Umidade</dt><dd>{{ fmt(out.hum, 0) }} %</dd>
              <dt>Ponto de orvalho</dt><dd>{{ fmt(out.dew, 0) }} °C</dd>
              <dt>Vento</dt><dd>{{ windText(out) }}</dd>
            </dl>
            <span class="src" :title="out.raw ?? undefined">{{ sourceText }}</span>
          </template>
          <p v-else-if="!outdoor?.located" class="empty">
            Defina a localização da estação em <code>grain.local</code> → Configurações para ver os dados de fora.
          </p>
          <p v-else class="empty">Aguardando a primeira coleta (acontece no próximo envio da estação).</p>
        </article>

        <article class="mode">
          <header><span class="tag">Indoor</span><span class="muted small">{{ data.device.place ? PLACE_LABEL[data.device.place].toLowerCase() : 'estação' }}</span></header>
          <template v-if="data.latest">
            <span class="v">{{ fmt(data.latest.temp) }}<small>°C</small></span>
            <span class="cond">umidade {{ fmt(data.latest.hum, 0) }} %</span>
            <dl class="kv">
              <dt>Ponto de orvalho</dt><dd>{{ fmt(data.latest.dew) }} °C</dd>
              <dt>Temp. em 24 h</dt><dd>{{ data.last24h?.n ? `${fmt(data.last24h.temp_min)} a ${fmt(data.last24h.temp_max)} °C` : '–' }}</dd>
              <dt>Umidade em 24 h</dt><dd>{{ data.last24h?.n ? `${fmt(data.last24h.hum_min, 0)} a ${fmt(data.last24h.hum_max, 0)} %` : '–' }}</dd>
              <dt v-if="out?.temp != null && data.latest.temp != null">Diferença</dt>
              <dd v-if="out?.temp != null && data.latest.temp != null">{{ data.latest.temp - out.temp >= 0 ? '+' : '' }}{{ fmt(data.latest.temp - out.temp) }} °C em relação a fora</dd>
            </dl>
            <span class="src">Média de 10 min iniciada às {{ hhmm(data.latest.ts) }}</span>
          </template>
          <p v-else class="empty">Aguardando a primeira leitura (a estação envia a cada 10 min).</p>
        </article>
      </section>

      <section class="row2">
        <article class="card">
          <span class="k">Pressão (barômetro da estação)</span>
          <span class="v sm">{{ fmt(data.latest?.pres_slp, 1) }}<small>hPa</small></span>
          <span class="sub" v-if="forecast?.pressure.trend">
            {{ trendArrow[forecast.pressure.trend] }} {{ forecast.pressure.trend }}
            ({{ (forecast.pressure.delta_3h ?? 0) >= 0 ? '+' : '' }}{{ fmt(forecast.pressure.delta_3h) }} hPa em 3 h)
          </span>
          <span class="sub" v-else>tendência disponível após 3 h de dados</span>
          <span class="sub" v-if="forecast?.pressure.outlook">{{ forecast.pressure.outlook }}</span>
          <span class="sub" v-if="outdoor?.metar?.pres">Aeroporto: QNH {{ fmt(outdoor.metar.pres, 0) }} hPa</span>
          <span class="sub" v-if="data.device.pres_cal?.auto && data.device.pres_cal.offset">
            Calibrada pelo aeroporto ({{ data.device.pres_cal.offset > 0 ? '+' : '' }}{{ fmt(data.device.pres_cal.offset, 2) }} hPa)
          </span>
        </article>
        <article class="card">
          <span class="k">Previsão{{ forecast?.location.label ? ` · ${forecast.location.label}` : '' }}</span>
          <template v-if="forecast?.today || forecast?.tomorrow">
            <div v-for="[label, d] in ([['Hoje', forecast.today], ['Amanhã', forecast.tomorrow]] as const)" :key="label" class="day">
              <template v-if="d">
                <b>{{ label }}</b>
                <span class="mono">{{ fmt(d.tmin, 0) }}° / {{ fmt(d.tmax, 0) }}°</span>
                <span>chuva {{ d.rain == null ? '–' : fmt(d.rain, 0) + ' %' }}</span>
                <span class="muted">{{ d.text }}</span>
              </template>
            </div>
            <span class="sub">Open-Meteo</span>
          </template>
          <span v-else class="sub">{{ forecast?.location.configured ? 'Previsão indisponível no momento.' : 'Defina a localização em grain.local para ver a previsão.' }}</span>
        </article>
      </section>

      <section class="cols">
        <div class="block">
          <h2>Últimas leituras (Indoor)</h2>
          <div class="tbl">
            <table>
              <thead>
                <tr><th>Início</th><th>Temp.</th><th>Umid.</th><th>Pressão</th><th>Orvalho</th></tr>
              </thead>
              <tbody>
                <tr v-for="r in data.recent" :key="r.ts">
                  <td class="mono">{{ hhmm(r.ts) }}</td>
                  <td>{{ fmt(r.temp) }} °C</td>
                  <td>{{ fmt(r.hum, 0) }} %</td>
                  <td>{{ fmt(r.pres_slp, 1) }}</td>
                  <td>{{ fmt(r.dew) }} °C</td>
                </tr>
                <tr v-if="!data.recent.length"><td colspan="5" class="muted">Sem leituras ainda.</td></tr>
              </tbody>
            </table>
          </div>
        </div>

        <div class="block">
          <h2>Estação</h2>
          <dl class="kv box">
            <dt>Local</dt><dd>{{ data.device.place ? PLACE_LABEL[data.device.place] : '–' }}</dd>
            <dt>Último envio</dt><dd>{{ ago(data.device.last_seen_at) }}</dd>
            <dt>Firmware</dt><dd>{{ data.device.status?.fw ?? '–' }}</dd>
            <dt>Sinal Wi-Fi</dt><dd>{{ data.device.status?.rssi != null ? data.device.status.rssi + ' dBm' : '–' }}</dd>
            <dt>Ligada há</dt><dd>{{ uptime(data.device.status?.uptime_s) }}</dd>
            <dt>Fila no ESP32</dt><dd>{{ data.device.status?.queue ?? '–' }}</dd>
          </dl>
          <p class="small"><NuxtLink to="/historico">Ver gráficos e histórico →</NuxtLink></p>
        </div>
      </section>
    </template>
  </main>
</template>

<style scoped>
.wrap { max-width: 1040px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 22px; }
h2 { font-size: 1.15rem; font-weight: 700; font-stretch: 108%; }
.small { font-size: 0.85rem; margin: 0; }
.modes { display: grid; grid-template-columns: repeat(auto-fit, minmax(290px, 1fr)); gap: 14px; }
.mode { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 16px 18px; display: grid; gap: 6px; align-content: start; min-width: 0; }
.mode header { display: flex; align-items: baseline; gap: 10px; }
.tag { font-family: var(--ff-display); font-weight: 800; font-stretch: 118%; font-size: 1.35rem; line-height: 1; }
.v { font-family: var(--ff-display); font-size: 3rem; font-weight: 760; font-stretch: 112%; font-variant-numeric: tabular-nums; line-height: 1.05; }
.v.sm { font-size: 2.2rem; }
.v small { font-size: 1rem; font-weight: 500; color: var(--muted); margin-left: 4px; }
.cond { font-size: 1rem; font-weight: 600; }
.src { font-family: var(--ff-mono); font-size: 0.74rem; color: var(--muted); }
.empty { margin: 4px 0 0; color: var(--muted); font-size: 0.92rem; }
.row2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(290px, 1fr)); gap: 14px; }
.card { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 16px 18px; display: grid; gap: 4px; align-content: start; min-width: 0; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.sub { font-size: 0.86rem; color: var(--muted); }
.day { display: grid; grid-template-columns: 64px auto auto 1fr; gap: 10px; align-items: baseline; padding: 6px 0; border-bottom: 1px solid var(--line); font-size: 0.95rem; }
.day:last-of-type { border-bottom: none; }
.day .muted { text-align: right; overflow-wrap: anywhere; }
@media (max-width: 420px) { .day { grid-template-columns: 58px auto 1fr; } .day .muted { grid-column: 1 / -1; text-align: left; } }
.kv { display: grid; grid-template-columns: auto 1fr; margin: 4px 0 2px; }
.kv dt, .kv dd { margin: 0; padding: 6px 0; border-bottom: 1px solid var(--line); }
.kv dt { color: var(--muted); font-size: 0.9rem; padding-right: 12px; }
.kv dd { text-align: right; font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
.kv dt:last-of-type, .kv dd:last-of-type { border-bottom: none; }
.kv.box { background: var(--surface); border: 1px solid var(--line); border-radius: 10px; margin: 0; }
.kv.box dt, .kv.box dd { padding: 8px 12px; }
.cols { display: grid; grid-template-columns: minmax(0, 1.6fr) minmax(0, 1fr); gap: 22px; }
@media (max-width: 760px) { .cols { grid-template-columns: 1fr; } }
.block { display: grid; gap: 10px; align-content: start; min-width: 0; }
.tbl { overflow-x: auto; border: 1px solid var(--line); border-radius: 10px; background: var(--surface); }
table { width: 100%; border-collapse: collapse; font-size: 0.92rem; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); white-space: nowrap; }
th { font-family: var(--ff-mono); font-size: 0.7rem; letter-spacing: 0.07em; text-transform: uppercase; color: var(--muted); background: var(--sunk); font-weight: 600; }
tr:last-child td { border-bottom: none; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice.crit { border-left-color: var(--crit); }
code { font-family: var(--ff-mono); font-size: 0.88em; background: var(--sunk); padding: 0.05em 0.35em; border-radius: 4px; }
</style>
