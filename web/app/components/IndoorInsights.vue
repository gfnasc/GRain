<script setup lang="ts">
// Card "No escritório": conforto, janela, ar seco, mofo e atraso térmico (GET /api/v1/indoor).
type Level = 'ok' | 'atenção' | 'alerta'
export type Indoor = {
  now: { ts: string; temp: number | null; hum: number | null; dew: number | null } | null
  comfort: { label: string; level: Level; temp: string; hum: string } | null
  dry: { level: Level; text: string } | null
  mold: { level: Level; hours_72h: number; text: string } | null
  window: { action: 'abrir' | 'fechar' | 'tanto faz'; reason: string } | null
  lag: { hours: number; follow: number; pairs: number; text: string } | null
  lag_needed_h: number | null
}
defineProps<{ data: Indoor | null; place: string }>()
const WINDOW = { abrir: { icon: '⇱', text: 'Abra a janela' }, fechar: { icon: '⇲', text: 'Mantenha a janela fechada' }, 'tanto faz': { icon: '↔', text: 'Janela: tanto faz' } } as const
</script>

<template>
  <article v-if="data?.now" class="indoor">
    <header>
      <span class="k">No {{ place }}</span>
      <span v-if="data.comfort" class="chip" :class="data.comfort.level">{{ data.comfort.label }}</span>
    </header>
    <div class="rows">
      <div v-if="data.window" class="row" :class="data.window.action === 'abrir' ? 'go' : ''">
        <span class="ic" aria-hidden="true">{{ WINDOW[data.window.action].icon }}</span>
        <div><b>{{ WINDOW[data.window.action].text }}</b><span>{{ data.window.reason }}</span></div>
      </div>
      <div v-if="data.dry" class="row" :class="data.dry.level">
        <span class="ic" aria-hidden="true">◌</span>
        <div><b>{{ data.dry.level === 'alerta' ? 'Ar muito seco' : 'Ar seco' }}</b><span>{{ data.dry.text }}</span></div>
      </div>
      <div v-if="data.mold" class="row" :class="data.mold.level">
        <span class="ic" aria-hidden="true">≋</span>
        <div><b>Mofo: {{ data.mold.level === 'ok' ? 'risco baixo' : data.mold.level === 'alerta' ? 'risco alto' : 'atenção' }}</b><span>{{ data.mold.text }}</span></div>
      </div>
      <div class="row">
        <span class="ic" aria-hidden="true">⏱</span>
        <div>
          <b>Atraso térmico</b>
          <span v-if="data.lag">{{ data.lag.text }}</span>
          <span v-else>Juntando dados: faltam cerca de {{ data.lag_needed_h }} h de medições aqui para calcular quanto o ambiente demora para acompanhar a temperatura de fora.</span>
        </div>
      </div>
    </div>
    <p class="foot">Faixas: temperatura agradável de 20 a 26 °C; umidade ideal de 40 a 60 %.</p>
  </article>
</template>

<style scoped>
.indoor { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 16px 18px; display: grid; gap: 12px; min-width: 0; }
header { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 8px; }
.k { font-family: var(--ff-mono); font-size: 0.72rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--muted); }
.chip { font-size: 0.82rem; font-weight: 650; padding: 3px 10px; border-radius: 999px; border: 1px solid var(--line); text-transform: capitalize; }
.chip.ok { border-color: var(--ok); color: var(--ok); }
.chip.atenção { border-color: var(--warn); color: var(--warn); }
.chip.alerta { border-color: var(--crit); color: var(--crit); }
.rows { display: grid; gap: 8px; }
.row { display: grid; grid-template-columns: 28px minmax(0, 1fr); gap: 10px; align-items: start; padding: 8px 10px; border-radius: 10px; background: var(--bg); border: 1px solid var(--line); }
.row.go { border-color: var(--ok); }
.row.atenção { border-color: var(--warn); }
.row.alerta { border-color: var(--crit); }
.ic { font-size: 1.2rem; line-height: 1.3; text-align: center; color: var(--muted); }
.row div { display: grid; gap: 2px; }
.row b { font-size: 0.95rem; }
.row span { font-size: 0.86rem; color: var(--muted); }
.foot { margin: 0; font-size: 0.78rem; color: var(--muted); }
</style>
