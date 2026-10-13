<script setup lang="ts">
// Aba Chuva: chance agora pelos três métodos, histórico das previsões com os trechos em que
// choveu de fato e o placar de acertos de cada método (últimos 30 dias).
import type { RainResp } from '~/components/RainNow.vue'

useHead({ title: 'Chuva · GRain' })

type Score = { method: 'm1' | 'm2' | 'm3'; horizon: number; n: number; events: number; brier: number | null; skill: number | null; hits: number; false_alarms: number; misses: number; reliability: { bin: number; n: number; freq: number }[] }
type ScoreResp = { days: number; min_n: number; min_events: number; pending: number; scores: Score[] }
type History = { from: string; to: string; h: number; points: { ts: number; m1: number | null; m2: number | null; m3: number | null }[]; rain: { from: number; to: number; sources: string[] }[] }

const range = ref<'24h' | '7d'>('24h')
const horizon = ref(1)
const { data: rain, refresh: refreshRain } = await useFetch<RainResp>('/api/v1/rain')
const { data: score } = await useFetch<ScoreResp>('/api/v1/rain/score')
const { data: hist, refresh: refreshHist } = await useFetch<History>('/api/v1/rain/history', { query: computed(() => ({ range: range.value, h: horizon.value })) })
const { data: latest } = await useFetch<{ device: { last_seen_at: string | null } | null }>('/api/v1/latest')



const METHODS = [
  { key: 'm1', name: 'M1 · Barômetro', color: '--series-m1', about: 'Queda da pressão da estação em 1 h e 3 h (descontada a variação diária normal) e nível da pressão.' },
  { key: 'm2', name: 'M2 · Barômetro + aeroporto', color: '--series-m2', about: 'M1 mais o METAR (chuva ou trovoada no aeroporto ou perto dele, nuvens CB/TCU, ar perto de saturar, teto baixo, rajadas) e o TAF.' },
  { key: 'm3', name: 'M3 · Modelo numérico', color: '--series-m3', about: 'Open-Meteo: probabilidade por hora, chuva prevista em blocos de 15 min e energia para trovoada (CAPE).' },
] as const

const now = computed(() => rain.value?.now ?? null)
const bestIdx = computed(() => (now.value ? now.value.best.map((b) => b.method) : []))
const topReason = (m: string) => {
  const v = now.value?.methods[m]
  if (!v?.length) return ''
  return v.reduce((a, b) => (b.prob > a.prob ? b : a)).reason
}
const xMin = computed(() => Math.floor(new Date(hist.value?.from ?? Date.now()).getTime() / 1000))
const xMax = computed(() => Math.floor(new Date(hist.value?.to ?? Date.now()).getTime() / 1000))
const rainPeriods = computed(() => (hist.value?.rain ?? []).map((r) => ({ from: r.from, to: r.to, label: 'chuva' })))

const scoreOf = (m: string, h: number) => score.value?.scores.find((s) => s.method === m && s.horizon === h) ?? null
const enough = (s: Score | null) => !!s && s.n >= (score.value?.min_n ?? 100) && s.events >= (score.value?.min_events ?? 5)
const totals = computed(() => {
  const s = score.value?.scores.filter((x) => x.horizon === 1) ?? []
  return { n: Math.max(0, ...s.map((x) => x.n)), events: Math.max(0, ...s.map((x) => x.events)) }
})
const fmt = (v: number | null | undefined, d = 2) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d })
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="latest?.device?.last_seen_at ?? null" />

    <SyncTimers :every="60" @refresh="refreshRain(); refreshHist()" />
    <RainNow :data="rain ?? null" @reported="refreshRain(); refreshHist()" />

    <section class="block">
      <h2>Os três métodos agora</h2>
      <p class="muted small">Do menos ao mais completo. O destaque de cada janela é o método de menor erro no placar; até o placar ter dados, é o modelo.</p>
      <div class="tbl">
        <table>
          <thead><tr><th>Método</th><th>1 h</th><th>2 h</th><th>3 h</th></tr></thead>
          <tbody>
            <tr v-for="m in METHODS" :key="m.key">
              <td>
                <span class="mname"><i :style="{ background: `var(${m.color})` }" />{{ m.name }}</span>
                <span class="reason">{{ now?.methods[m.key] ? topReason(m.key) : (m.key === 'm2' ? 'sem METAR recente do aeroporto' : 'sem dados ainda') }}</span>
              </td>
              <td v-for="h in 3" :key="h" class="num" :class="{ best: bestIdx[h - 1] === m.key }">
                {{ now?.methods[m.key]?.[h - 1] ? Math.round(now.methods[m.key]![h - 1]!.prob * 100) + ' %' : '–' }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="block">
      <div class="block-head">
        <h2>Previsões × o que aconteceu</h2>
        <div class="chips" role="group" aria-label="Janela">
          <button v-for="h in 3" :key="h" type="button" class="chip" :aria-pressed="horizon === h" @click="horizon = h">{{ h }} h</button>
          <span class="sep" />
          <button type="button" class="chip" :aria-pressed="range === '24h'" @click="range = '24h'">24 h</button>
          <button type="button" class="chip" :aria-pressed="range === '7d'" @click="range = '7d'">7 dias</button>
        </div>
      </div>
      <ClientOnly>
        <TimeChart
          v-if="hist?.points.length"
          :title="`Chance de chuva na próxima ${horizon === 1 ? 'hora' : `${horizon} horas`}`"
          unit="%"
          :decimals="0"
          :min-span="100"
          :y-range="[0, 100]"
          :step="600"
          :x-min="xMin"
          :x-max="xMax"
          :points="hist.points"
          :periods="rainPeriods"
          :series="METHODS.map((m) => ({ key: m.key, label: m.key.toUpperCase(), color: m.color }))"
        />
        <p v-else class="notice">As previsões começam a aparecer aqui depois dos primeiros envios da estação.</p>
      </ClientOnly>
      <p class="muted small">Trecho hachurado: choveu (METAR do aeroporto, aviso “Está chovendo aqui” ou, sem METAR, o modelo).</p>
    </section>

    <section class="block">
      <h2>Placar dos últimos {{ score?.days ?? 30 }} dias</h2>
      <p class="muted small">
        Cada previsão é conferida quando a janela termina. <b>Erro (Brier)</b>: 0 é perfeito, 0,25 equivale a chutar 50 %.
        <b>Habilidade</b>: acima de 0 é melhor que repetir a frequência de chuva do período.
        <b>Acertos / alarmes falsos / perdidas</b>: considerando “vai chover” a partir de 50 %.
      </p>
      <p v-if="totals.n < (score?.min_n ?? 100) || totals.events < (score?.min_events ?? 5)" class="notice">
        <strong>Ainda juntando dados</strong>
        <span>{{ totals.n }} previsões de 1 h conferidas, {{ totals.events }} com chuva ({{ score?.pending ?? 0 }} aguardando a janela terminar). O placar fica confiável a partir de {{ score?.min_n ?? 100 }} previsões e {{ score?.min_events ?? 5 }} chuvas por método; com a estação chuvosa, deve levar algumas semanas.</span>
      </p>
      <div class="tbl">
        <table>
          <thead>
            <tr><th>Método</th><th>Janela</th><th>Erro</th><th>Habilidade</th><th>Acertos</th><th>Alarmes falsos</th><th>Perdidas</th><th>Conferidas</th><th>Com chuva</th></tr>
          </thead>
          <tbody>
            <template v-for="m in METHODS" :key="m.key">
              <tr v-for="h in 3" :key="h" :class="{ dim: !enough(scoreOf(m.key, h)) }">
                <td v-if="h === 1" :rowspan="3"><span class="mname"><i :style="{ background: `var(${m.color})` }" />{{ m.key.toUpperCase() }}</span></td>
                <td>{{ h }} h</td>
                <td class="num">{{ fmt(scoreOf(m.key, h)?.brier, 3) }}</td>
                <td class="num">{{ fmt(scoreOf(m.key, h)?.skill) }}</td>
                <td class="num">{{ scoreOf(m.key, h)?.hits ?? '–' }}</td>
                <td class="num">{{ scoreOf(m.key, h)?.false_alarms ?? '–' }}</td>
                <td class="num">{{ scoreOf(m.key, h)?.misses ?? '–' }}</td>
                <td class="num">{{ scoreOf(m.key, h)?.n ?? 0 }}</td>
                <td class="num">{{ scoreOf(m.key, h)?.events ?? 0 }}</td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>
    </section>

    <section class="block">
      <h2>Como funciona</h2>
      <ul class="about">
        <li v-for="m in METHODS" :key="m.key"><b>{{ m.name }}</b>: {{ m.about }}</li>
        <li><b>Conferência</b>: choveu na janela se o METAR do aeroporto registrou chuva (fora “nas proximidades”) ou se alguém tocou em “Está chovendo aqui”; sem METAR, vale a chuva medida pelo modelo. O aviso local vale mais que o aeroporto, que fica a alguns quilômetros.</li>
        <li><b>Próximo passo</b> (fase 18): um método combinado (M4) que aprende os pesos com este placar.</li>
      </ul>
    </section>
  </main>
</template>

<style scoped>
.wrap { max-width: 1040px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 22px; }
.block { display: grid; gap: 10px; min-width: 0; }
.block-head { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 10px; }
h2 { font-size: 1.15rem; font-weight: 700; font-stretch: 108%; }
.small { font-size: 0.85rem; margin: 0; max-width: 80ch; }
.chips { display: flex; flex-wrap: wrap; gap: 6px; align-items: center; }
.chip { font: inherit; font-size: 0.85rem; font-weight: 600; border: 1px solid var(--line); background: var(--surface); color: var(--fg); padding: 5px 12px; border-radius: 999px; cursor: pointer; }
.chip[aria-pressed='true'] { background: var(--fg); color: var(--bg); border-color: var(--fg); }
.chip:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.sep { width: 1px; height: 20px; background: var(--line); margin: 0 4px; }
.tbl { overflow-x: auto; border: 1px solid var(--line); border-radius: 10px; background: var(--surface); }
table { width: 100%; border-collapse: collapse; font-size: 0.92rem; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
th { font-family: var(--ff-mono); font-size: 0.7rem; letter-spacing: 0.07em; text-transform: uppercase; color: var(--muted); background: var(--sunk); font-weight: 600; white-space: nowrap; }
tr:last-child td { border-bottom: none; }
td.num { font-variant-numeric: tabular-nums; white-space: nowrap; }
td.best { font-weight: 760; box-shadow: inset 0 -2px 0 var(--accent); }
tr.dim td { color: var(--muted); }
.mname { display: inline-flex; align-items: center; gap: 8px; font-weight: 650; white-space: nowrap; }
.mname i { width: 12px; height: 3px; border-radius: 2px; display: inline-block; }
.reason { display: block; font-size: 0.82rem; color: var(--muted); margin-top: 2px; max-width: 60ch; }
.about { margin: 0; padding-left: 1.1em; display: grid; gap: 6px; font-size: 0.92rem; max-width: 85ch; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice span { font-size: 0.9rem; color: var(--muted); }
</style>
