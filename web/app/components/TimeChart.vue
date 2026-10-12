<script setup lang="ts">
// Gráfico de série temporal (uPlot) com faixa mín.–máx. opcional,
// lacunas visíveis quando faltam dados, eixo de tempo no fuso configurado,
// cruz de leitura e tooltip ao passar o mouse/dedo.
import uPlot from 'uplot'
import 'uplot/dist/uPlot.min.css'

type Point = Record<string, number | null> & { ts: number }
type SeriesDef = { key: string; label: string; color: string; bandMin?: string; bandMax?: string }

const props = defineProps<{
  points: Point[]
  series: SeriesDef[]
  unit: string
  decimals: number
  step: number
  title: string
  // Faixa mínima do eixo vertical: evita ampliar variações mínimas (ruído) em gráficos planos.
  minSpan: number
  // Período pedido (epoch s): o eixo do tempo cobre o período inteiro, mesmo sem dados.
  xMin: number
  xMax: number
}>()

const tz = useRuntimeConfig().public.tz as string
const el = ref<HTMLDivElement>()
const tip = ref<HTMLDivElement>()
let plot: uPlot | null = null
let ro: ResizeObserver | null = null
let mql: MediaQueryList | null = null

const cssVar = (name: string) => getComputedStyle(el.value!).getPropertyValue(name).trim()

function alpha(hex: string, a: number) {
  const h = hex.replace('#', '')
  const n = parseInt(h.length === 3 ? h.split('').map((c) => c + c).join('') : h, 16)
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`
}

const fmtNum = (v: number | null | undefined) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: props.decimals, maximumFractionDigits: props.decimals, useGrouping: false })

const span = computed(() => props.xMax - props.xMin)

const fmtTick = (ts: number) =>
  new Date(ts * 1000).toLocaleString('pt-BR', span.value <= 2 * 86400
    ? { hour: '2-digit', minute: '2-digit', timeZone: tz }
    : { day: '2-digit', month: '2-digit', timeZone: tz })

const fmtWhen = (ts: number) => {
  const d = new Date(ts * 1000)
  const opt: Intl.DateTimeFormatOptions = { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz }
  const start = d.toLocaleString('pt-BR', opt)
  if (props.step <= 600) return start
  const end = new Date((ts + props.step) * 1000).toLocaleString('pt-BR', props.step >= 86400
    ? { day: '2-digit', month: '2-digit', timeZone: tz }
    : { hour: '2-digit', minute: '2-digit', timeZone: tz })
  return props.step >= 86400 ? new Date(ts * 1000).toLocaleDateString('pt-BR', { timeZone: tz }) : `${start}–${end}`
}

// Monta as colunas do uPlot, inserindo null onde faltam intervalos (lacuna visível).
function buildData() {
  const xs: number[] = []
  const cols: (number | null)[][] = []
  const keys: string[] = []
  for (const s of props.series) {
    keys.push(s.key)
    if (s.bandMin && s.bandMax) keys.push(s.bandMin, s.bandMax)
  }
  keys.forEach(() => cols.push([]))
  let prev: number | null = null
  for (const p of props.points) {
    if (prev !== null && p.ts - prev > props.step * 1.5) {
      xs.push(prev + props.step)
      cols.forEach((c) => c.push(null))
    }
    xs.push(p.ts)
    keys.forEach((k, i) => cols[i]!.push(p[k] ?? null))
    prev = p.ts
  }
  return { data: [xs, ...cols] as uPlot.AlignedData, keys }
}

function tooltipPlugin(): uPlot.Plugin {
  return {
    hooks: {
      setCursor: (u) => {
        const t = tip.value
        if (!t) return
        const idx = u.cursor.idx
        if (idx == null || u.data[0][idx] == null) {
          t.hidden = true
          return
        }
        const ts = u.data[0][idx] as number
        const rows = props.series
          .map((s) => {
            const si = u.series.findIndex((x) => (x as any)._key === s.key)
            const v = u.data[si]?.[idx] as number | null
            let extra = ''
            if (s.bandMin && s.bandMax && props.step > 600) {
              const lo = u.data[u.series.findIndex((x) => (x as any)._key === s.bandMin)]?.[idx] as number | null
              const hi = u.data[u.series.findIndex((x) => (x as any)._key === s.bandMax)]?.[idx] as number | null
              if (lo != null && hi != null) extra = `<span class="range">${fmtNum(lo)} a ${fmtNum(hi)}</span>`
            }
            return `<div class="row"><i style="background:${cssVar(s.color)}"></i><span>${s.label}</span><b>${fmtNum(v)} ${props.unit}</b>${extra}</div>`
          })
          .join('')
        const allNull = props.series.every((s) => {
          const si = u.series.findIndex((x) => (x as any)._key === s.key)
          return u.data[si]?.[idx] == null
        })
        t.innerHTML = `<div class="when">${fmtWhen(ts)}</div>${allNull ? '<div class="row muted">sem dados</div>' : rows}`
        t.hidden = false
        const left = u.cursor.left ?? 0
        const w = t.offsetWidth
        const plotLeft = u.over.offsetLeft
        const x = Math.min(Math.max(plotLeft + left - w / 2, 0), u.root.clientWidth - w)
        t.style.left = `${x}px`
      },
    },
  }
}

function build() {
  if (!el.value) return
  plot?.destroy()
  const { data, keys } = buildData()
  const muted = cssVar('--muted')
  const line = cssVar('--line')
  const surface = cssVar('--surface')

  const series: uPlot.Series[] = [{}]
  const bands: uPlot.Band[] = []
  let i = 1
  for (const s of props.series) {
    const color = cssVar(s.color)
    const main: any = { label: s.label, stroke: color, width: 2, points: { show: data[0].length < 4, size: 8, fill: color, stroke: surface }, spanGaps: false }
    main._key = s.key
    series.push(main)
    const mainIdx = i++
    if (s.bandMin && s.bandMax) {
      const lo: any = { stroke: 'transparent', width: 0, points: { show: false } }
      const hi: any = { stroke: 'transparent', width: 0, points: { show: false } }
      lo._key = s.bandMin
      hi._key = s.bandMax
      series.push(lo, hi)
      const loIdx = i++
      const hiIdx = i++
      if (props.step > 600) bands.push({ series: [hiIdx, loIdx], fill: alpha(color, 0.16) })
      void mainIdx
    }
  }
  void keys

  const opts: uPlot.Options = {
    width: el.value.clientWidth,
    height: 240,
    padding: [12, 22, 0, 0],
    legend: { show: false },
    cursor: { y: false, points: { size: 8, width: 2, fill: surface } },
    scales: {
      x: { time: true, range: () => [props.xMin, props.xMax] },
      y: {
        range: (_u, min, max) => {
          if (min == null || max == null) return [0, 1]
          const mid = (min + max) / 2
          const half = Math.max((max - min) / 2, props.minSpan / 2) * 1.1
          return [mid - half, mid + half]
        },
      },
    },
    series,
    bands,
    axes: [
      {
        stroke: muted,
        grid: { show: false },
        ticks: { stroke: line, size: 4 },
        font: '11px "JetBrains Mono", ui-monospace, monospace',
        values: (_u, splits) => splits.map((v) => fmtTick(v)),
        space: 70,
      },
      {
        stroke: muted,
        grid: { stroke: line, width: 1 },
        ticks: { show: false },
        font: '11px "JetBrains Mono", ui-monospace, monospace',
        // Casas decimais conforme o passo entre as marcas, para não repetir rótulos.
        values: (_u, splits, _i, _s, incr) => {
          const d = Math.max(0, Math.min(2, -Math.floor(Math.log10(incr || 1))))
          return splits.map((v) => v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d, useGrouping: false }))
        },
        size: 52,
      },
    ],
    plugins: [tooltipPlugin()],
  }
  plot = new uPlot(opts, data, el.value)
  plot.over.addEventListener('mouseleave', () => tip.value && (tip.value.hidden = true))
}

onMounted(() => {
  build()
  ro = new ResizeObserver(() => plot && el.value && plot.setSize({ width: el.value.clientWidth, height: 240 }))
  ro.observe(el.value!)
  mql = window.matchMedia('(prefers-color-scheme: dark)')
  mql.addEventListener('change', build)
})
onBeforeUnmount(() => {
  ro?.disconnect()
  mql?.removeEventListener('change', build)
  plot?.destroy()
})
watch(() => [props.points, props.step, props.xMin, props.xMax], build)
</script>

<template>
  <figure class="chart">
    <figcaption>
      <h3>{{ title }}</h3>
      <ul v-if="series.length > 1" class="legend">
        <li v-for="s in series" :key="s.key"><i :style="{ background: `var(${s.color})` }" />{{ s.label }}</li>
      </ul>
      <span v-else-if="step > 600 && series[0]?.bandMin" class="legend-note">linha = média · faixa = mín. a máx.</span>
    </figcaption>
    <div class="plot-wrap">
      <div ref="el" class="plot" role="img" :aria-label="`Gráfico: ${title}`" />
      <div ref="tip" class="tip" hidden />
    </div>
  </figure>
</template>

<style scoped>
.chart { margin: 0; background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 14px 8px; display: grid; gap: 8px; min-width: 0; }
figcaption { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: 6px 14px; }
h3 { font-size: 1rem; font-weight: 700; font-stretch: 106%; }
.legend { display: flex; gap: 14px; list-style: none; margin: 0; padding: 0; font-size: 0.85rem; color: var(--muted); }
.legend li { display: inline-flex; align-items: center; gap: 6px; }
.legend i { width: 16px; height: 3px; border-radius: 2px; display: inline-block; }
.legend-note { font-size: 0.8rem; color: var(--muted); }
.plot-wrap { position: relative; min-width: 0; }
.plot { width: 100%; min-width: 0; }
.tip {
  position: absolute;
  top: 0;
  pointer-events: none;
  background: var(--surface);
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 8px 10px;
  font-size: 0.82rem;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.12);
  z-index: 2;
  min-width: 150px;
}
.tip :deep(.when) { font-family: var(--ff-mono); font-size: 0.75rem; color: var(--muted); margin-bottom: 4px; }
.tip :deep(.row) { display: grid; grid-template-columns: 10px 1fr auto; align-items: center; gap: 6px; }
.tip :deep(.row i) { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.tip :deep(.row b) { font-variant-numeric: tabular-nums; color: var(--fg); }
.tip :deep(.row .range) { grid-column: 2 / 4; font-size: 0.75rem; color: var(--muted); text-align: right; }
.tip :deep(.muted) { color: var(--muted); display: block; }
.plot :deep(.u-over) { cursor: crosshair; }
</style>
