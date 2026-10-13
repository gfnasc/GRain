<script setup lang="ts">
// Mostrador de um teste longo (robustez de 72 h, chuva de 30 dias): quanto falta para terminar
// com sucesso, a barra de progresso e a lista de critérios.
export type TrialState = 'andamento' | 'falhou' | 'sucesso' | 'aguardando'

const props = defineProps<{
  title: string
  state: TrialState
  startedAt: string | null
  endsAt: string | null
  checks: { ok: boolean | null; label: string }[]
  waiting?: string
  note?: string
}>()

const tz = useRuntimeConfig().public.tz as string
const when = (iso: string) => new Date(iso).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz })

const now = ref(Date.now())
let tick: ReturnType<typeof setInterval> | undefined
onMounted(() => { tick = setInterval(() => (now.value = Date.now()), 1000) })
onBeforeUnmount(() => clearInterval(tick))

const left = computed(() => (props.endsAt ? Math.max(0, new Date(props.endsAt).getTime() - now.value) : null))
const progress = computed(() => {
  if (!props.startedAt || !props.endsAt) return 0
  const a = new Date(props.startedAt).getTime(), b = new Date(props.endsAt).getTime()
  return Math.min(100, Math.max(0, ((now.value - a) / (b - a)) * 100))
})
function span(ms: number) {
  const s = Math.floor(ms / 1000)
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60), x = s % 60
  if (d) return `${d} d ${String(h).padStart(2, '0')} h ${String(m).padStart(2, '0')} min`
  return `${h} h ${String(m).padStart(2, '0')} min ${String(x).padStart(2, '0')} s`
}
const LABEL: Record<TrialState, string> = { andamento: 'Em andamento', falhou: 'Com falha', sucesso: 'Concluído com sucesso', aguardando: 'Aguardando início' }
</script>

<template>
  <section class="trial" :class="state">
    <header>
      <h3>{{ title }}</h3>
      <span class="chip">{{ LABEL[state] }}</span>
    </header>

    <template v-if="state !== 'aguardando' && endsAt">
      <div class="left">
        <span v-if="state === 'sucesso'" class="big">Passou</span>
        <template v-else>
          <span class="k">{{ state === 'falhou' ? 'Tempo até o fim do prazo' : 'Falta para terminar com sucesso' }}</span>
          <span class="big" aria-live="off">{{ span(left ?? 0) }}</span>
        </template>
        <span class="muted small">Começou em {{ when(startedAt!) }} · {{ state === 'sucesso' ? 'terminou' : 'termina' }} em {{ when(endsAt) }}</span>
      </div>
      <div class="bar" role="progressbar" :aria-valuenow="Math.round(progress)" aria-valuemin="0" aria-valuemax="100" :aria-label="`${title}: ${Math.round(progress)} % do prazo`">
        <i :style="{ width: `${progress}%` }" />
      </div>
    </template>
    <p v-else class="muted small">{{ waiting }}</p>

    <ul class="checks">
      <li v-for="c in checks" :key="c.label" :class="c.ok === true ? 'ok' : c.ok === false ? 'bad' : 'wait'">
        <span aria-hidden="true">{{ c.ok === true ? '✓' : c.ok === false ? '✕' : '…' }}</span>{{ c.label }}
      </li>
    </ul>
    <p v-if="note" class="muted small">{{ note }}</p>
  </section>
</template>

<style scoped>
.trial { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 10px; min-width: 0; }
.trial.falhou { border-color: var(--crit); }
.trial.sucesso { border-color: var(--ok); }
header { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 8px; }
h3 { font-size: 1.05rem; font-weight: 700; font-stretch: 106%; }
.chip { font-size: 0.8rem; font-weight: 650; padding: 3px 10px; border-radius: 999px; border: 1px solid var(--line); }
.andamento .chip { border-color: var(--accent); color: var(--accent); }
.falhou .chip { border-color: var(--crit); color: var(--crit); }
.sucesso .chip { border-color: var(--ok); color: var(--ok); }
.left { display: grid; gap: 2px; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.big { font-family: var(--ff-display); font-size: 1.9rem; font-weight: 760; font-stretch: 110%; line-height: 1.1; font-variant-numeric: tabular-nums; }
.bar { height: 8px; border-radius: 4px; background: var(--sunk); overflow: hidden; }
.bar i { display: block; height: 100%; border-radius: 4px; background: var(--accent); }
.falhou .bar i { background: var(--crit); }
.sucesso .bar i { background: var(--ok); }
.small { font-size: 0.85rem; margin: 0; }
.checks { list-style: none; margin: 0; padding: 0; display: grid; gap: 6px; }
.checks li { display: flex; gap: 8px; align-items: baseline; font-size: 0.93rem; }
.checks li span { width: 18px; flex: none; text-align: center; font-weight: 700; }
.checks li.ok span { color: var(--ok); }
.checks li.bad span { color: var(--crit); }
.checks li.wait span { color: var(--warn); }
@media (max-width: 420px) { .big { font-size: 1.5rem; } }
</style>
