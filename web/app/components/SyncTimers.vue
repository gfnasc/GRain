<script setup lang="ts">
// Contadores das próximas atualizações (estação, chuva, METAR, modelo e a própria página).
// Também é o relógio da página: emite "refresh" a cada `every` segundos.
type Slot = { last: number | null; next: number | null }
type Sync = { now: number; interval_s: number; station: Slot; rain: Slot; metar: Slot; model: Slot; taf: Slot }

const props = withDefaults(defineProps<{ every?: number }>(), { every: 60 })
const emit = defineEmits<{ refresh: [] }>()

const { data, refresh } = await useFetch<Sync>('/api/v1/sync')
const offset = ref(0) // relógio do servidor − relógio do navegador
watch(data, (d) => d && (offset.value = d.now - Date.now()), { immediate: true })

const now = ref(Date.now())
const pageAt = ref(Date.now() + props.every * 1000)
let timer: ReturnType<typeof setInterval> | undefined
onMounted(() => {
  timer = setInterval(() => {
    now.value = Date.now()
    if (now.value >= pageAt.value) {
      pageAt.value = now.value + props.every * 1000
      emit('refresh')
      refresh()
    }
  }, 1000)
})
onBeforeUnmount(() => clearInterval(timer))

const server = computed(() => now.value + offset.value)
function left(t: number | null | undefined, pending = 'chegando…') {
  if (!t) return '–'
  const s = Math.round((t - server.value) / 1000)
  if (s <= 0) return pending
  const m = Math.floor(s / 60)
  return m >= 60 ? `${Math.floor(m / 60)} h ${String(m % 60).padStart(2, '0')}` : `${m}:${String(s % 60).padStart(2, '0')}`
}
const items = computed(() => {
  const d = data.value
  return [
    { k: 'Página', v: left(pageAt.value - offset.value, 'atualizando…'), title: `Recarrega os dados a cada ${props.every} s` },
    { k: 'Média da estação', v: left(d?.station.next), title: `A estação envia uma média a cada ${(d?.interval_s ?? 300) / 60} min` },
    { k: 'Previsão de chuva', v: left(d?.rain.next), title: 'Recalculada a cada 10 min' },
    { k: 'METAR', v: left(d?.metar.next, 'aguardando o aeroporto'), title: 'O aeroporto publica de hora em hora (ou antes, se o tempo mudar); a nuvem confere a cada envio' },
    { k: 'Modelo e TAF', v: left(d?.model.next), title: 'Open-Meteo e TAF a cada 30 min' },
  ]
})
</script>

<template>
  <section class="timers" aria-label="Próximas atualizações">
    <span class="lbl">Próximas atualizações</span>
    <span v-for="i in items" :key="i.k" class="t" :title="i.title"><b>{{ i.k }}</b><span class="mono">{{ i.v }}</span></span>
  </section>
</template>

<style scoped>
.timers { display: flex; flex-wrap: wrap; align-items: center; gap: 6px 14px; padding: 8px 12px; border: 1px dashed var(--line); border-radius: 10px; font-size: 0.82rem; color: var(--muted); }
.lbl { font-family: var(--ff-mono); font-size: 0.68rem; letter-spacing: 0.08em; text-transform: uppercase; }
.t { display: inline-flex; gap: 6px; align-items: baseline; }
.t b { font-weight: 600; color: var(--muted); }
.t .mono { color: var(--fg); font-variant-numeric: tabular-nums; }
</style>
