<script setup lang="ts">
useHead({ title: 'GWeather' })

type Reading = { ts: number; temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null }
type Latest = {
  device: { id: string; name: string; last_seen_at: string | null; status: Record<string, any> | null } | null
  latest: Reading | null
  recent: Reading[]
  last24h: { temp_min: number | null; temp_max: number | null; hum_min: number | null; hum_max: number | null; n: number } | null
}

const { data, refresh, error } = await useFetch<Latest>('/api/v1/latest')

// Atualiza a cada minuto; o ESP32 envia médias de 10 min.
let timer: ReturnType<typeof setInterval> | undefined
const now = ref(Date.now())
onMounted(() => {
  timer = setInterval(() => {
    now.value = Date.now()
    refresh()
  }, 60_000)
})
onBeforeUnmount(() => clearInterval(timer))

const fmt = (v: number | null | undefined, d = 1) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d, useGrouping: false })
const tz = useRuntimeConfig().public.tz
const hhmm = (ts: number) =>
  new Date(ts * 1000).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: tz })

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
      <section v-if="!data.latest" class="notice">
        <strong>Aguardando a primeira leitura.</strong>
        <span>A estação envia a média a cada 10 minutos.</span>
      </section>

      <template v-else>
        <p class="muted when">Média de 10 min iniciada às <span class="mono">{{ hhmm(data.latest.ts) }}</span></p>
        <section class="cards">
          <article class="card">
            <span class="k">Temperatura</span>
            <span class="v">{{ fmt(data.latest.temp) }}<small>°C</small></span>
            <span class="sub" v-if="data.last24h?.n">24 h: {{ fmt(data.last24h.temp_min) }} a {{ fmt(data.last24h.temp_max) }} °C</span>
          </article>
          <article class="card">
            <span class="k">Umidade</span>
            <span class="v">{{ fmt(data.latest.hum, 0) }}<small>%</small></span>
            <span class="sub" v-if="data.last24h?.n">24 h: {{ fmt(data.last24h.hum_min, 0) }} a {{ fmt(data.last24h.hum_max, 0) }} %</span>
          </article>
          <article class="card">
            <span class="k">Pressão (nível do mar)</span>
            <span class="v">{{ fmt(data.latest.pres_slp, 0) }}<small>hPa</small></span>
            <span class="sub">Ponto de orvalho {{ fmt(data.latest.dew) }} °C</span>
          </article>
        </section>
      </template>

      <section class="cols">
        <div class="block">
          <h2>Últimas leituras</h2>
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
          <dl class="kv">
            <dt>Identificação</dt><dd class="mono">{{ data.device.id }}</dd>
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
.when { margin: 0; font-size: 0.9rem; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; }
.card { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 16px 18px; display: grid; gap: 4px; min-width: 0; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.v { font-family: var(--ff-display); font-size: 2.6rem; font-weight: 760; font-stretch: 112%; font-variant-numeric: tabular-nums; line-height: 1.1; }
.v small { font-size: 1rem; font-weight: 500; color: var(--muted); margin-left: 4px; }
.sub { font-size: 0.85rem; color: var(--muted); }
.cols { display: grid; grid-template-columns: minmax(0, 1.6fr) minmax(0, 1fr); gap: 22px; }
@media (max-width: 760px) { .cols { grid-template-columns: 1fr; } }
.block { display: grid; gap: 10px; align-content: start; min-width: 0; }
.tbl { overflow-x: auto; border: 1px solid var(--line); border-radius: 10px; background: var(--surface); }
table { width: 100%; border-collapse: collapse; font-size: 0.92rem; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); white-space: nowrap; }
th { font-family: var(--ff-mono); font-size: 0.7rem; letter-spacing: 0.07em; text-transform: uppercase; color: var(--muted); background: var(--sunk); font-weight: 600; }
tr:last-child td { border-bottom: none; }
.kv { display: grid; grid-template-columns: auto 1fr; gap: 0; margin: 0; background: var(--surface); border: 1px solid var(--line); border-radius: 10px; overflow: hidden; }
.kv dt, .kv dd { margin: 0; padding: 8px 12px; border-bottom: 1px solid var(--line); }
.kv dt { color: var(--muted); font-size: 0.9rem; }
.kv dd { text-align: right; font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
.kv dt:nth-last-of-type(1), .kv dd:last-of-type { border-bottom: none; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice.crit { border-left-color: var(--crit); }
.small { font-size: 0.85rem; margin: 0; }
code { font-family: var(--ff-mono); font-size: 0.88em; background: var(--sunk); padding: 0.05em 0.35em; border-radius: 4px; }
</style>
