<script setup lang="ts">
useHead({ title: 'Histórico · GWeather' })

type Point = { ts: number; temp: number | null; temp_min: number | null; temp_max: number | null; hum: number | null; hum_min: number | null; hum_max: number | null; pres: number | null; pres_min: number | null; pres_max: number | null; dew: number | null; n: number }
type Extreme = { v: number; ts: number } | null
type Stat = { min: Extreme; max: Extreme; avg: number | null }
type Series = { from: string; to: string; step_s: number; points: Point[]; summary: { n: number; expected: number; temp: Stat; hum: Stat; pres: Stat } | null }
type Day = { day: string; temp_min: number | null; temp_max: number | null; temp_avg: number | null; hum_min: number | null; hum_max: number | null; hum_avg: number | null; pres_min: number | null; pres_max: number | null; pres_avg: number | null; n: number }

const route = useRoute()
const router = useRouter()
const tz = useRuntimeConfig().public.tz as string

const presets = [
  { key: '24h', label: '24 h' },
  { key: '7d', label: '7 dias' },
  { key: '30d', label: '30 dias' },
]

// Estado do filtro vive na URL (?range=7d ou ?from=...&to=...), para poder compartilhar o link.
const query = computed<Record<string, string>>(() => {
  const q = route.query
  if (typeof q.from === 'string' && typeof q.to === 'string') return { from: q.from, to: q.to }
  return { range: typeof q.range === 'string' ? q.range : '24h' }
})
const isCustom = computed(() => 'from' in query.value)

const today = new Date().toLocaleDateString('sv-SE', { timeZone: tz })
const customFrom = ref(typeof route.query.from === 'string' ? route.query.from : new Date(Date.now() - 6 * 86400_000).toLocaleDateString('sv-SE', { timeZone: tz }))
const customTo = ref(typeof route.query.to === 'string' ? route.query.to : today)
const showCustom = ref(isCustom.value)

function pick(key: string) {
  showCustom.value = false
  router.replace({ query: { range: key } })
}
function applyCustom() {
  if (customFrom.value && customTo.value && customFrom.value <= customTo.value) {
    router.replace({ query: { from: customFrom.value, to: customTo.value } })
  }
}

const { data: series, pending, error } = await useFetch<Series>('/api/v1/readings', { query })
const { data: daily } = await useFetch<{ days: Day[] }>('/api/v1/daily', { query })
const { data: latest } = await useFetch<{ device: { last_seen_at: string | null } | null }>('/api/v1/latest')

const csvHref = computed(() => `/api/v1/readings.csv?${new URLSearchParams(query.value)}`)
const csvExcelHref = computed(() => `/api/v1/readings.csv?${new URLSearchParams({ ...query.value, excel: '1' })}`)

const fmt = (v: number | null | undefined, d = 1) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d, useGrouping: false })
const when = (ts: number) =>
  new Date(ts * 1000).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz })
const dayLabel = (d: string) => {
  const [y, m, dd] = d.split('-').map(Number)
  return new Date(Date.UTC(y!, m! - 1, dd!, 12)).toLocaleDateString('pt-BR', { weekday: 'short', day: '2-digit', month: '2-digit', timeZone: 'UTC' })
}
const stepLabel = computed(() => {
  const s = series.value?.step_s ?? 600
  return s <= 600 ? 'médias de 10 min' : s < 86400 ? `médias de ${s / 3600} h` : 'médias diárias'
})
const xMin = computed(() => Math.floor(new Date(series.value?.from ?? Date.now()).getTime() / 1000))
const xMax = computed(() => Math.floor(new Date(series.value?.to ?? Date.now()).getTime() / 1000))
const coverage = computed(() => {
  const s = series.value?.summary
  if (!s || !s.expected) return null
  return Math.min(100, Math.round((s.n / s.expected) * 100))
})
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="latest?.device?.last_seen_at ?? null" />

    <section class="filters" aria-label="Período">
      <div class="chips" role="group" aria-label="Período">
        <button
          v-for="p in presets"
          :key="p.key"
          type="button"
          class="chip"
          :aria-pressed="!isCustom && !showCustom && query.range === p.key"
          @click="pick(p.key)"
        >
          {{ p.label }}
        </button>
        <button type="button" class="chip" :aria-pressed="isCustom || showCustom" @click="showCustom = !showCustom">Período…</button>
      </div>
      <form v-if="showCustom || isCustom" class="custom" @submit.prevent="applyCustom">
        <label for="from">De</label>
        <input id="from" v-model="customFrom" type="date" :max="today" required>
        <label for="to">até</label>
        <input id="to" v-model="customTo" type="date" :max="today" required>
        <button class="btn" type="submit">Aplicar</button>
      </form>
      <div class="downloads">
        <a class="btn" :href="csvHref">Baixar CSV</a>
        <a class="btn" :href="csvExcelHref" title="Separador ; e vírgula decimal">CSV para Excel</a>
      </div>
    </section>

    <p v-if="error" class="notice crit">{{ error.data?.message || 'Não foi possível carregar o histórico.' }}</p>

    <template v-else-if="series?.summary">
      <p class="muted meta">
        <span>{{ stepLabel }}</span>
        <span v-if="coverage !== null"> · {{ series.summary.n }} médias recebidas ({{ coverage }}% do período)</span>
        <span v-if="pending"> · atualizando…</span>
      </p>

      <section class="stats">
        <article class="stat">
          <span class="k">Temperatura (°C)</span>
          <div class="mm">
            <div><small>mín.</small><b>{{ fmt(series.summary.temp.min?.v) }}</b><span v-if="series.summary.temp.min">{{ when(series.summary.temp.min.ts) }}</span></div>
            <div><small>máx.</small><b>{{ fmt(series.summary.temp.max?.v) }}</b><span v-if="series.summary.temp.max">{{ when(series.summary.temp.max.ts) }}</span></div>
          </div>
          <span class="avg">média {{ fmt(series.summary.temp.avg) }} °C</span>
        </article>
        <article class="stat">
          <span class="k">Umidade (%)</span>
          <div class="mm">
            <div><small>mín.</small><b>{{ fmt(series.summary.hum.min?.v, 0) }}</b><span v-if="series.summary.hum.min">{{ when(series.summary.hum.min.ts) }}</span></div>
            <div><small>máx.</small><b>{{ fmt(series.summary.hum.max?.v, 0) }}</b><span v-if="series.summary.hum.max">{{ when(series.summary.hum.max.ts) }}</span></div>
          </div>
          <span class="avg">média {{ fmt(series.summary.hum.avg, 0) }} %</span>
        </article>
        <article class="stat">
          <span class="k">Pressão ao nível do mar (hPa)</span>
          <div class="mm">
            <div><small>mín.</small><b>{{ fmt(series.summary.pres.min?.v) }}</b><span v-if="series.summary.pres.min">{{ when(series.summary.pres.min.ts) }}</span></div>
            <div><small>máx.</small><b>{{ fmt(series.summary.pres.max?.v) }}</b><span v-if="series.summary.pres.max">{{ when(series.summary.pres.max.ts) }}</span></div>
          </div>
          <span class="avg">média {{ fmt(series.summary.pres.avg) }} hPa</span>
        </article>
      </section>

      <section v-if="!series.points.length" class="notice">
        <strong>Sem dados neste período.</strong>
        <span>Escolha outro período ou aguarde novas médias da estação.</span>
      </section>

      <ClientOnly v-else>
        <section class="charts">
          <TimeChart
            title="Temperatura e ponto de orvalho"
            unit="°C"
            :decimals="1"
            :min-span="4"
            :step="series.step_s"
            :x-min="xMin"
            :x-max="xMax"
            :points="series.points"
            :series="[
              { key: 'temp', label: 'Temperatura', color: '--series-temp', bandMin: 'temp_min', bandMax: 'temp_max' },
              { key: 'dew', label: 'Ponto de orvalho', color: '--series-dew' },
            ]"
          />
          <TimeChart
            title="Umidade relativa"
            unit="%"
            :decimals="0"
            :min-span="10"
            :step="series.step_s"
            :x-min="xMin"
            :x-max="xMax"
            :points="series.points"
            :series="[{ key: 'hum', label: 'Umidade', color: '--series-hum', bandMin: 'hum_min', bandMax: 'hum_max' }]"
          />
          <TimeChart
            title="Pressão ao nível do mar"
            unit="hPa"
            :decimals="1"
            :min-span="4"
            :step="series.step_s"
            :x-min="xMin"
            :x-max="xMax"
            :points="series.points"
            :series="[{ key: 'pres', label: 'Pressão', color: '--series-pres', bandMin: 'pres_min', bandMax: 'pres_max' }]"
          />
        </section>
      </ClientOnly>

      <section v-if="daily?.days.length" class="block">
        <h2>Resumo por dia</h2>
        <div class="tbl">
          <table>
            <thead>
              <tr>
                <th>Dia</th>
                <th>Temp. mín.</th><th>Temp. máx.</th>
                <th>Umid. mín.</th><th>Umid. máx.</th>
                <th>Pressão média</th>
                <th>Médias</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="d in daily.days" :key="d.day">
                <td>{{ dayLabel(d.day) }}</td>
                <td>{{ fmt(d.temp_min) }} °C</td>
                <td>{{ fmt(d.temp_max) }} °C</td>
                <td>{{ fmt(d.hum_min, 0) }} %</td>
                <td>{{ fmt(d.hum_max, 0) }} %</td>
                <td>{{ fmt(d.pres_avg) }} hPa</td>
                <td class="muted">{{ d.n }}/144</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>
    </template>
  </main>
</template>

<style scoped>
.wrap { max-width: 1040px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 20px; }
.filters { display: flex; flex-wrap: wrap; align-items: center; gap: 10px 16px; }
.chips { display: flex; flex-wrap: wrap; gap: 6px; }
.chip { font: inherit; font-size: 0.9rem; font-weight: 600; border: 1px solid var(--line); background: var(--surface); color: var(--fg); padding: 6px 14px; border-radius: 999px; cursor: pointer; }
.chip[aria-pressed='true'] { background: var(--fg); color: var(--bg); border-color: var(--fg); }
.chip:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.custom { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.custom label { font-size: 0.85rem; color: var(--muted); }
.custom input { font: inherit; padding: 5px 8px; border: 1px solid var(--line); border-radius: 8px; background: var(--surface); color: var(--fg); }
.downloads { display: flex; gap: 8px; margin-left: auto; }
.downloads .btn { text-decoration: none; font-size: 0.9rem; }
.meta { margin: 0; font-size: 0.88rem; }
.stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 12px; }
.stat { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 8px; min-width: 0; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.mm { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.mm div { display: grid; gap: 1px; }
.mm small { font-size: 0.75rem; color: var(--muted); }
.mm b { font-family: var(--ff-display); font-size: 1.45rem; white-space: nowrap; font-weight: 760; font-stretch: 110%; font-variant-numeric: tabular-nums; }
.mm span { font-family: var(--ff-mono); font-size: 0.72rem; color: var(--muted); }
.avg { font-size: 0.85rem; color: var(--muted); }
.charts { display: grid; gap: 14px; }
.block { display: grid; gap: 10px; }
h2 { font-size: 1.15rem; font-weight: 700; font-stretch: 108%; }
.tbl { overflow-x: auto; border: 1px solid var(--line); border-radius: 10px; background: var(--surface); }
table { width: 100%; border-collapse: collapse; font-size: 0.92rem; font-variant-numeric: tabular-nums; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); white-space: nowrap; }
th { font-family: var(--ff-mono); font-size: 0.7rem; letter-spacing: 0.07em; text-transform: uppercase; color: var(--muted); background: var(--sunk); font-weight: 600; }
tr:last-child td { border-bottom: none; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice.crit { border-left-color: var(--crit); }
@media (max-width: 560px) { .downloads { margin-left: 0; } }
</style>
