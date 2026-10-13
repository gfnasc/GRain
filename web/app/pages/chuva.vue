<script setup lang="ts">
// Aba Chuva: chance agora pelos cinco métodos, o teste de 30 dias, o histórico das previsões
// com os trechos em que choveu de fato, o placar de cada método e o que os treinados aprenderam.
import type { RainMethod, RainResp } from '~/components/RainNow.vue'
import type { TrialState } from '~/components/TrialCountdown.vue'

useHead({ title: 'Chuva · GRain' })

type Score = { method: RainMethod; horizon: number; n: number; events: number; brier: number | null; skill: number | null; hits: number; false_alarms: number; misses: number; reliability: { bin: number; n: number; freq: number }[] }
type Learned = { method: 'm2t' | 'm4'; horizons: { h: number; n: number; events: number; trained_at: string | null; weights: { key: string; prior: number; value: number }[] }[] }
type Trial = { started_at: string | null; ends_at: string | null; fw?: string; fw_needed?: string; state: TrialState; days: number; min_coverage?: number; coverage?: { m2t: number; m4: number }; rain_hours?: number; min_events?: number; scores: Score[] }
type ScoreResp = { days: number; min_n: number; min_events: number; pending: number; scores: Score[]; trial: Trial; learned: Learned[] }
type History = { from: string; to: string; h: number; points: Record<string, number | null>[]; rain: { from: number; to: number; sources: string[] }[] }

const range = ref<'24h' | '7d'>('24h')
const horizon = ref(1)
const { data: rain, refresh: refreshRain } = await useFetch<RainResp>('/api/v1/rain')
const { data: score, refresh: refreshScore } = await useFetch<ScoreResp>('/api/v1/rain/score')
const { data: hist, refresh: refreshHist } = await useFetch<History>('/api/v1/rain/history', { query: computed(() => ({ range: range.value, h: horizon.value })) })
const { data: latest } = await useFetch<{ device: { last_seen_at: string | null } | null }>('/api/v1/latest')

const METHODS = [
  { key: 'm1', name: 'M1 · Barômetro', color: '--series-m1', about: 'Queda da pressão da estação em 1 h e 3 h (descontada a variação diária normal) e nível da pressão. Pesos fixos.' },
  { key: 'm2', name: 'M2 · Barômetro + aeroporto', color: '--series-m2', about: 'M1 mais o METAR (chuva ou trovoada no aeroporto ou perto dele, nuvens CB/TCU, ar perto de saturar, teto baixo, rajadas) e o TAF. Pesos fixos.' },
  { key: 'm2t', name: 'M2T · Barômetro + aeroporto, treinado', color: '--series-m2t', about: 'Os mesmos dados do M2, com o peso de cada sinal aprendido pelo placar. Começou igual ao M2 e muda só o que as conferências sustentam.' },
  { key: 'm3', name: 'M3 · Modelo numérico', color: '--series-m3', about: 'Open-Meteo: probabilidade por hora, chuva prevista em blocos de 15 min e energia para trovoada (CAPE). Sem ajuste local.' },
  { key: 'm4', name: 'M4 · Combinado, treinado', color: '--series-m4', about: 'Estação + aeroporto + modelo, com pesos aprendidos pelo placar. Começou como a média do M2 e do M3 e aprende quanto confiar em cada um.' },
] as const

const TERM_LABEL: Record<string, string> = {
  bias: 'Chance de base', queda3: 'Pressão caindo (3 h)', subida3: 'Pressão subindo (3 h)', queda1: 'Pressão caindo (1 h)', baixa: 'Pressão baixa', alta: 'Pressão alta',
  chuva_aero: 'Chovendo no aeroporto', proximidades: 'Chuva/trovoada perto do aeroporto', cb: 'Nuvens de trovoada (CB)', tcu: 'Nuvens TCU', saturado: 'Ar perto de saturar',
  seco: 'Ar seco', teto: 'Teto baixo', rajada: 'Rajadas', taf: 'TAF prevê chuva', modelo: 'Modelo numérico (M3)',
}

const now = computed(() => rain.value?.now ?? null)
const bestIdx = computed(() => (now.value ? now.value.best.map((b) => b.method) : []))
const topReason = (m: string) => {
  const v = now.value?.methods[m]
  if (!v?.length) return ''
  return v.reduce((a, b) => (b.prob > a.prob ? b : a)).reason
}
const missingText = (m: string) => (m === 'm2' || m === 'm2t' ? 'sem METAR recente do aeroporto' : m === 'm4' ? 'sem previsão do modelo nesta rodada' : 'sem dados ainda')

// Gráfico: por padrão os três que disputam (M2T, M3, M4); M1 e M2 entram quando ligados.
const shown = ref<string[]>(['m2t', 'm3', 'm4'])
const toggle = (k: string) => (shown.value = shown.value.includes(k) ? shown.value.filter((x) => x !== k) : [...shown.value, k])
const chartSeries = computed(() => METHODS.filter((m) => shown.value.includes(m.key)).map((m) => ({ key: m.key, label: m.key.toUpperCase(), color: m.color })))
const xMin = computed(() => Math.floor(new Date(hist.value?.from ?? Date.now()).getTime() / 1000))
const xMax = computed(() => Math.floor(new Date(hist.value?.to ?? Date.now()).getTime() / 1000))
const rainPeriods = computed(() => (hist.value?.rain ?? []).map((r) => ({ from: r.from, to: r.to, label: 'chuva' })))

const scoreOf = (m: string, h: number, list = score.value?.scores) => list?.find((s) => s.method === m && s.horizon === h) ?? null
const enough = (s: Score | null) => !!s && s.n >= (score.value?.min_n ?? 100) && s.events >= (score.value?.min_events ?? 5)
const totals = computed(() => {
  const s = score.value?.scores.filter((x) => x.horizon === 1) ?? []
  return { n: Math.max(0, ...s.map((x) => x.n)), events: Math.max(0, ...s.map((x) => x.events)) }
})
const fmt = (v: number | null | undefined, d = 2) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d })

// Teste de 30 dias.
const trial = computed(() => score.value?.trial ?? null)
const trialLeader = computed(() => {
  const s = (trial.value?.scores ?? []).filter((x) => x.horizon === 1 && x.brier != null && x.n > 0)
  if (!s.length || !s.some((x) => x.events > 0)) return null
  return s.sort((a, b) => a.brier! - b.brier!)[0]!
})
const trialChecks = computed(() => {
  const t = trial.value
  if (!t || !t.started_at) return [{ ok: null, label: 'Começa quando a estação mandar a primeira mensagem do firmware ' + (t?.fw_needed ?? '0.14.0') }]
  const minCov = t.min_coverage ?? 90
  const cov = t.coverage ?? { m2t: 0, m4: 0 }
  const ev = t.rain_hours ?? 0, minEv = t.min_events ?? 5
  return [
    { ok: t.state === 'sucesso' ? true : null, label: `${t.days} dias de previsões` },
    { ok: Math.min(cov.m2t, cov.m4) >= minCov ? true : t.state === 'falhou' ? false : null, label: `M2T e M4 em pelo menos ${minCov} % das rodadas (agora: M2T ${cov.m2t} %, M4 ${cov.m4} %)` },
    { ok: ev >= minEv ? true : null, label: `Pelo menos ${minEv} horas com chuva conferidas para o placar comparar (até agora: ${ev})` },
    beats('m4', ['m1', 'm2', 'm3'], ev >= minEv, 'M4 com erro menor que M1, M2 e M3 nas três janelas'),
    beats('m2t', ['m2'], ev >= minEv, 'M2T com erro menor que o M2 de pesos fixos nas três janelas'),
  ]
})
// Aceite da fase: o método vence os outros (menor Brier desde o começo do teste) em 1, 2 e 3 h.
// Só decide com chuva suficiente; antes disso fica "…".
function beats(m: string, others: string[], ready: boolean, label: string) {
  const list = trial.value?.scores ?? []
  const wins = [1, 2, 3].map((h) => {
    const a = scoreOf(m, h, list)?.brier
    const rest = others.map((o) => scoreOf(o, h, list)?.brier).filter((v): v is number => v != null)
    return a != null && rest.length === others.length ? rest.every((v) => a < v) : null
  })
  const now = wins.filter((w) => w === true).length
  const done = trial.value?.state === 'sucesso' || trial.value?.state === 'falhou'
  const ok = !ready || wins.includes(null) ? null : now === 3 ? true : done ? false : null
  return { ok, label: `${label}${ready ? ` (por enquanto vence em ${now} de 3)` : ''}` }
}
const trialNote = computed(() => {
  const l = trialLeader.value
  if (!l) return 'Placar do teste: aparece quando houver chuva conferida desde o começo.'
  return `Liderando na janela de 1 h desde o começo do teste: ${l.method.toUpperCase()} (erro ${fmt(l.brier, 3)}, ${l.n} conferidas, ${l.events} com chuva).`
})

// O que os treinados aprenderam (na janela escolhida).
const learnH = ref(1)
const learnedRows = computed(() => {
  const L = score.value?.learned ?? []
  const a = L.find((x) => x.method === 'm2t')?.horizons.find((x) => x.h === learnH.value)
  const b = L.find((x) => x.method === 'm4')?.horizons.find((x) => x.h === learnH.value)
  const keys = [...new Set([...(a?.weights ?? []), ...(b?.weights ?? [])].map((w) => w.key))]
  return {
    m2t: a ?? null,
    m4: b ?? null,
    rows: keys.map((k) => ({ key: k, label: TERM_LABEL[k] ?? k, m2t: a?.weights.find((w) => w.key === k) ?? null, m4: b?.weights.find((w) => w.key === k) ?? null })),
  }
})
// Peso relativo ao de partida (×1,00 = não mudou); a base aparece em pontos de logit.
const rel = (w: { key: string; prior: number; value: number } | null) => {
  if (!w) return { text: '–', moved: false }
  if (w.key === 'bias') return { text: `${fmt(w.prior)} → ${fmt(w.value)}`, moved: Math.abs(w.value - w.prior) >= 0.05 }
  const r = w.prior ? w.value / w.prior : 0
  return { text: `×${fmt(r)}`, moved: Math.abs(r - 1) >= 0.05 }
}
const tz = useRuntimeConfig().public.tz as string
const when = (iso: string | null | undefined) => (iso ? new Date(iso).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz }) : '–')

function refreshAll() {
  refreshRain()
  refreshHist()
  refreshScore()
}
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="latest?.device?.last_seen_at ?? null" />

    <SyncTimers :every="60" @refresh="refreshRain(); refreshHist()" />
    <RainNow :data="rain ?? null" @reported="refreshAll" />

    <TrialCountdown
      v-if="trial"
      :title="`Teste de ${trial.days} dias dos métodos treinados`"
      :state="trial.state"
      :started-at="trial.started_at"
      :ends-at="trial.ends_at"
      :checks="trialChecks"
      waiting="O teste começa junto com o de robustez, na gravação do firmware 0.14.0."
      :note="trialNote"
    />

    <section class="block">
      <h2>Os cinco métodos agora</h2>
      <p class="muted small">Do menos ao mais completo. O destaque de cada janela é o método de menor erro no placar; até o placar ter dados, é o modelo.</p>
      <div class="tbl">
        <table>
          <thead><tr><th>Método</th><th>1 h</th><th>2 h</th><th>3 h</th></tr></thead>
          <tbody>
            <tr v-for="m in METHODS" :key="m.key">
              <td>
                <span class="mname"><i :style="{ background: `var(${m.color})` }" />{{ m.name }}</span>
                <span class="reason">{{ now?.methods[m.key] ? topReason(m.key) : missingText(m.key) }}</span>
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
        <div class="chips" role="group" aria-label="Janela e período">
          <button v-for="h in 3" :key="h" type="button" class="chip" :aria-pressed="horizon === h" @click="horizon = h">{{ h }} h</button>
          <span class="sep" />
          <button type="button" class="chip" :aria-pressed="range === '24h'" @click="range = '24h'">24 h</button>
          <button type="button" class="chip" :aria-pressed="range === '7d'" @click="range = '7d'">7 dias</button>
        </div>
      </div>
      <div class="chips" role="group" aria-label="Métodos no gráfico">
        <button v-for="m in METHODS" :key="m.key" type="button" class="chip mchip" :aria-pressed="shown.includes(m.key)" @click="toggle(m.key)">
          <i :style="{ background: `var(${m.color})` }" />{{ m.key.toUpperCase() }}
        </button>
      </div>
      <ClientOnly>
        <TimeChart
          v-if="hist?.points.length && chartSeries.length"
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
          :series="chartSeries"
        />
        <p v-else-if="!chartSeries.length" class="notice">Escolha pelo menos um método acima.</p>
        <p v-else class="notice">As previsões começam a aparecer aqui depois dos primeiros envios da estação.</p>
      </ClientOnly>
      <p class="muted small">Trecho hachurado: choveu (METAR do aeroporto, aviso “Está chovendo aqui” ou, sem METAR, o modelo). Um “Não choveu hoje” apaga a chuva do aeroporto do dia.</p>
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
      <div class="block-head">
        <h2>O que os treinados aprenderam</h2>
        <div class="chips" role="group" aria-label="Janela">
          <button v-for="h in 3" :key="h" type="button" class="chip" :aria-pressed="learnH === h" @click="learnH = h">{{ h }} h</button>
        </div>
      </div>
      <p class="muted small">
        Peso de cada sinal em relação ao de partida (×1,00 = não mudou; abaixo de 1, o placar mostrou que o sinal exagerava; acima, que valia mais).
        Treinados com {{ learnedRows.m2t?.n ?? 0 }} conferências (M2T) e {{ learnedRows.m4?.n ?? 0 }} (M4), {{ learnedRows.m4?.events ?? 0 }} com chuva · refeito a cada 3 h (último: {{ when(learnedRows.m4?.trained_at) }}).
        Com pouca chuva conferida, os pesos ficam perto do original.
      </p>
      <div class="tbl">
        <table>
          <thead><tr><th>Sinal</th><th>M2T</th><th>M4</th></tr></thead>
          <tbody>
            <tr v-for="r in learnedRows.rows" :key="r.key">
              <td>{{ r.label }}</td>
              <td class="num" :class="{ moved: rel(r.m2t).moved }">{{ rel(r.m2t).text }}</td>
              <td class="num" :class="{ moved: rel(r.m4).moved }">{{ rel(r.m4).text }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="block">
      <h2>Como funciona</h2>
      <ul class="about">
        <li v-for="m in METHODS" :key="m.key"><b>{{ m.name }}</b>: {{ m.about }}</li>
        <li><b>Conferência</b>: choveu na janela se alguém tocou em “Está chovendo aqui” ou se o METAR do aeroporto registrou chuva (fora “nas proximidades”); sem METAR, vale a chuva medida pelo modelo. O aviso local vale mais que o aeroporto, que fica a alguns quilômetros: “Não choveu hoje” tira do placar a chuva que o aeroporto registrou no dia.</li>
        <li><b>Treino</b>: regressão logística com os sinais de cada rodada conferida (últimos 60 dias), puxada para os pesos de partida. Usa uma rodada a cada 20 min, valendo 1/3, porque as previsões de 10 em 10 min se sobrepõem.</li>
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
.mchip { display: inline-flex; align-items: center; gap: 6px; }
.mchip i { width: 12px; height: 3px; border-radius: 2px; display: inline-block; }
.mchip[aria-pressed='false'] { color: var(--muted); }
.mchip[aria-pressed='false'] i { opacity: 0.4; }
.sep { width: 1px; height: 20px; background: var(--line); margin: 0 4px; }
.tbl { overflow-x: auto; border: 1px solid var(--line); border-radius: 10px; background: var(--surface); }
table { width: 100%; border-collapse: collapse; font-size: 0.92rem; }
th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
th { font-family: var(--ff-mono); font-size: 0.7rem; letter-spacing: 0.07em; text-transform: uppercase; color: var(--muted); background: var(--sunk); font-weight: 600; white-space: nowrap; }
tr:last-child td { border-bottom: none; }
td.num { font-variant-numeric: tabular-nums; white-space: nowrap; }
td.best { font-weight: 760; box-shadow: inset 0 -2px 0 var(--accent); }
td.moved { font-weight: 700; }
tr.dim td { color: var(--muted); }
.mname { display: inline-flex; align-items: center; gap: 8px; font-weight: 650; white-space: nowrap; }
.mname i { width: 12px; height: 3px; border-radius: 2px; display: inline-block; }
.reason { display: block; font-size: 0.82rem; color: var(--muted); margin-top: 2px; max-width: 60ch; }
.about { margin: 0; padding-left: 1.1em; display: grid; gap: 6px; font-size: 0.92rem; max-width: 85ch; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice span { font-size: 0.9rem; color: var(--muted); }
</style>
