<script setup lang="ts">
// Card "Chuva nas próximas horas": chance em 1 h, 2 h e 3 h pelo método em destaque, o motivo
// e os botões "Está chovendo aqui" e "Não choveu hoje" (viram verificação para o placar e o treino).
export type RainMethod = 'm1' | 'm2' | 'm2t' | 'm3' | 'm4'
type Best = { method: RainMethod; prob: number; level: string; reason: string }
export type RainResp = {
  located: boolean
  now: { issued_at: string; best: Best[]; chosen_by: 'placar' | 'padrão'; methods: Record<string, { prob: number; reason: string }[]> } | null
  reports: { ts: string; via: string }[]
  today?: { marked_at: string | null; metar_rain: string[] }
}

const props = defineProps<{ data: RainResp | null }>()
const emit = defineEmits<{ reported: [] }>()

const tz = useRuntimeConfig().public.tz as string
const hhmm = (iso: string) => new Date(iso).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: tz })
const METHOD_NAME = { m1: 'barômetro', m2: 'barômetro + aeroporto', m2t: 'barômetro + aeroporto, treinado', m3: 'modelo numérico', m4: 'combinado, treinado' } as const

const best = computed(() => props.data?.now?.best ?? [])
const top = computed(() => best.value.reduce<Best | null>((a, b) => (!a || b.prob > a.prob ? b : a), null))
const methodsText = computed(() => {
  const ms = [...new Set(best.value.map((b) => b.method))]
  return ms.map((m) => `${m.toUpperCase()} · ${METHOD_NAME[m]}`).join(', ')
})

const sending = ref(false)
const msg = ref('')
async function report() {
  sending.value = true
  msg.value = ''
  try {
    const r = await $fetch<{ ts: string }>('/api/v1/rain/report', { method: 'POST' })
    msg.value = `Registrado às ${hhmm(r.ts)}. Obrigado: isso entra no placar.`
    emit('reported')
  } catch {
    msg.value = 'Não foi possível registrar agora.'
  } finally {
    sending.value = false
  }
}
const lastReport = computed(() => props.data?.reports?.[0] ?? null)

// "Não choveu hoje": o aeroporto às vezes registra chuva que não chega aqui (ou chuva fraca
// que não molha o chão). O aviso tira essas chuvas do placar e do treino do dia.
const today = computed(() => props.data?.today ?? null)
const metarToday = computed(() => (today.value?.metar_rain ?? []).map(hhmm))
async function noRain(undo = false) {
  if (!undo && !confirm('Marcar que não choveu aqui hoje? As chuvas que o aeroporto registrou desde a meia-noite deixam de contar no placar e no treino.')) return
  sending.value = true
  msg.value = ''
  try {
    await $fetch('/api/v1/rain/report', { method: 'POST', body: { kind: 'nao_choveu_hoje', undo } })
    msg.value = undo ? 'Aviso “Não choveu hoje” desfeito.' : 'Registrado: hoje não choveu aqui. As previsões do dia foram conferidas de novo.'
    emit('reported')
  } catch {
    msg.value = 'Não foi possível registrar agora.'
  } finally {
    sending.value = false
  }
}
</script>

<template>
  <article class="rain">
    <header>
      <div>
        <span class="k">Chuva nas próximas horas</span>
        <span v-if="data?.now" class="src">{{ methodsText }} · calculado às {{ hhmm(data.now.issued_at) }}</span>
      </div>
      <div class="btns">
        <button type="button" class="btn" :disabled="sending" @click="report">☂ Está chovendo aqui</button>
        <button v-if="!today?.marked_at" type="button" class="btn ghost" :disabled="sending" @click="noRain()">☀ Não choveu hoje</button>
      </div>
    </header>

    <div v-if="data?.now" class="tiles">
      <div v-for="(b, i) in best" :key="i" class="tile" :class="b.level">
        <span class="h">{{ i + 1 }} h</span>
        <span class="p">{{ Math.round(b.prob * 100) }}<small>%</small></span>
        <span class="bar" aria-hidden="true"><i :style="{ width: `${Math.round(b.prob * 100)}%` }" /></span>
        <span class="lvl">{{ b.level }}</span>
      </div>
    </div>
    <p v-else class="empty">
      {{ data && !data.located ? 'Defina a localização da estação em gwethr.local para calcular a chance de chuva.' : 'A primeira previsão sai no próximo envio da estação (a cada 5 min).' }}
    </p>

    <p v-if="top" class="why">{{ top.reason }}</p>
    <p v-if="msg" class="msg" role="status">{{ msg }}</p>
    <p v-else-if="lastReport" class="msg muted">Último aviso de chuva: {{ hhmm(lastReport.ts) }} ({{ lastReport.via === 'estacao' ? 'pela estação' : 'pelo painel' }})</p>
    <p v-if="today?.marked_at" class="msg muted">
      Marcado às {{ hhmm(today.marked_at) }}: hoje não choveu aqui{{ metarToday.length ? ` (o aeroporto registrou chuva às ${metarToday.join(', ')}, que não conta)` : '' }}.
      <button type="button" class="link" :disabled="sending" @click="noRain(true)">Desfazer</button>
    </p>
    <p v-else-if="metarToday.length" class="msg muted">O aeroporto registrou chuva hoje às {{ metarToday.join(', ') }}. Se não choveu aqui, toque em “Não choveu hoje”.</p>
  </article>
</template>

<style scoped>
.rain { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 16px 18px; display: grid; gap: 12px; min-width: 0; }
header { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: start; gap: 10px; }
header > div { display: grid; gap: 2px; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.btns { display: flex; flex-wrap: wrap; gap: 8px; }
.btn.ghost { background: transparent; }
.link { font: inherit; background: none; border: none; padding: 0; color: var(--accent); text-decoration: underline; cursor: pointer; }
.src { font-family: var(--ff-mono); font-size: 0.72rem; color: var(--muted); }
.tiles { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; }
.tile { border: 1px solid var(--line); border-radius: 10px; padding: 10px 12px; display: grid; gap: 4px; background: var(--bg); min-width: 0; }
.h { font-family: var(--ff-mono); font-size: 0.78rem; color: var(--muted); }
.p { font-family: var(--ff-display); font-size: 2.2rem; font-weight: 760; font-stretch: 112%; line-height: 1; font-variant-numeric: tabular-nums; }
.p small { font-size: 0.95rem; font-weight: 500; color: var(--muted); margin-left: 2px; }
.bar { height: 6px; border-radius: 3px; background: var(--sunk); overflow: hidden; }
.bar i { display: block; height: 100%; border-radius: 3px; background: var(--muted); }
.tile.média .bar i { background: var(--warn); }
.tile.alta .bar i { background: var(--crit); }
.tile.alta { border-color: var(--crit); }
.lvl { font-size: 0.85rem; font-weight: 650; text-transform: capitalize; }
.why { margin: 0; font-size: 0.92rem; }
.msg { margin: 0; font-size: 0.85rem; }
.empty { margin: 0; color: var(--muted); font-size: 0.92rem; }
@media (max-width: 420px) { .p { font-size: 1.7rem; } .tile { padding: 8px; } }
</style>
