<script setup lang="ts">
// Aba Estação: estado da estação e cópia da configuração enviada por ela.
// Somente leitura; a configuração é feita na página local do ESP32 (grain.local).
useHead({ title: 'Estação · GRain' })

type StationResp = {
  device: { id: string; name: string; last_seen_at: string | null; status: Record<string, any> | null; place: 'sacada' | 'escritorio' | null }
  config: StationConfig | null
  config_at: string | null
}
type Latest = {
  latest: { temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null } | null
  last24h: { temp_min: number | null; temp_max: number | null; hum_min: number | null; hum_max: number | null } | null
}

const { data, error } = await useFetch<StationResp>('/api/v1/station')
const { data: latest } = await useFetch<Latest>('/api/v1/latest')
const tz = useRuntimeConfig().public.tz as string

const cfg = computed(() => data.value?.config ?? null)
const pages = computed(() => cfg.value?.display.pages ?? [])

// Prévia gira no ritmo configurado; tocar numa página fixa nela por 15 s.
const previewIndex = ref(0)
let spin: ReturnType<typeof setInterval> | undefined
let pausedUntil = 0
function startSpin() {
  clearInterval(spin)
  if (!cfg.value) return
  spin = setInterval(() => {
    if (Date.now() < pausedUntil || !pages.value.length) return
    previewIndex.value = (previewIndex.value + 1) % pages.value.length
  }, cfg.value.display.page_s * 1000)
}
function preview(i: number) {
  previewIndex.value = i
  pausedUntil = Date.now() + 15_000
}
onMounted(startSpin)
onBeforeUnmount(() => clearInterval(spin))

const PLACE_LABEL = { sacada: 'Sacada', escritorio: 'Escritório' } as const
const fmt = (v: number | null | undefined, d = 1) =>
  v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: d, maximumFractionDigits: d, useGrouping: false })
const signed = (v: number, unit: string) => (v === 0 ? 'sem ajuste' : `${v > 0 ? '+' : ''}${fmt(v)} ${unit}`)
const when = (iso: string | null) =>
  iso ? new Date(iso).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz }) : '–'
function uptime(s?: number) {
  if (!s) return '–'
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60)
  return `${d ? d + 'd ' : ''}${h}h ${String(m).padStart(2, '0')}m`
}
const fieldsText = (p: PageConfig) => (p.fields ?? []).map((f) => FIELD_LABELS[f]).join(', ')
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="data?.device.last_seen_at ?? null" />

    <div class="intro">
      <h2>Estação</h2>
      <p class="muted">
        Configuração atual, como a estação informou. Para alterar, abra <code>http://grain.local</code> num aparelho na rede de casa
        (a senha da estação aparece no comando <code>status</code> do console).
      </p>
    </div>

    <p v-if="error" class="notice crit">{{ error.data?.message || 'Não foi possível carregar a estação.' }}</p>

    <template v-else-if="data">
      <section v-if="!cfg" class="notice">
        <strong>A estação ainda não enviou a configuração.</strong>
        <span>Ela manda uma cópia junto com as médias (firmware 0.9.0 ou mais novo). Aguarde o próximo envio ou use “Sincronizar agora” na página local.</span>
      </section>

      <div v-else class="layout">
        <div class="col">
          <section class="block">
            <div class="block-head">
              <h3>Páginas do display</h3>
              <span class="muted small">{{ pages.length }} · {{ cfg.display.page_s }} s cada</span>
            </div>
            <ol class="pages">
              <li v-for="(p, i) in pages" :key="i" :class="{ active: i === previewIndex }">
                <button type="button" class="num" :aria-label="`Ver prévia da página ${i + 1}`" @click="preview(i)">{{ i + 1 }}</button>
                <div>
                  <b>{{ PAGE_LABELS[p.type] }}</b>
                  <span v-if="p.type === 'fields'" class="muted small"> · {{ fieldsText(p) }}</span>
                </div>
              </li>
            </ol>
          </section>

          <section class="block">
            <h3>Comportamento</h3>
            <dl class="kv">
              <dt>Brilho</dt><dd>{{ Math.round((cfg.display.contrast / 255) * 100) }}%</dd>
              <dt>Modo noturno</dt>
              <dd>{{ cfg.display.night.enabled ? `${cfg.display.night.mode === 'off' ? 'apaga' : 'escurece'} das ${cfg.display.night.from} às ${cfg.display.night.to}` : 'desligado' }}</dd>
            </dl>
          </section>

          <section class="block">
            <h3>Calibração</h3>
            <dl class="kv">
              <dt>Temperatura</dt><dd>{{ signed(cfg.calibration.temp, '°C') }}</dd>
              <dt>Umidade</dt><dd>{{ signed(cfg.calibration.hum, '%') }}</dd>
              <dt>Pressão</dt>
              <dd v-if="cfg.calibration.pres_auto">
                pelo aeroporto: {{ cfg.calibration.pres_auto_offset ? `${cfg.calibration.pres_auto_offset > 0 ? '+' : ''}${fmt(cfg.calibration.pres_auto_offset, 2)} hPa` : 'aguardando o cálculo' }}
              </dd>
              <dd v-else>{{ signed(cfg.calibration.pres, 'hPa') }} (manual)</dd>
            </dl>
          </section>

          <section class="block">
            <h3>Localização</h3>
            <dl class="kv">
              <dt>Nome</dt><dd>{{ cfg.location.label || '–' }}</dd>
              <dt>Coordenadas</dt>
              <dd v-if="cfg.location.lat != null">
                <a :href="`https://www.openstreetmap.org/?mlat=${cfg.location.lat}&mlon=${cfg.location.lon}#map=14/${cfg.location.lat}/${cfg.location.lon}`" target="_blank" rel="noopener">
                  {{ fmt(cfg.location.lat, 4) }}, {{ fmt(cfg.location.lon, 4) }}
                </a>
              </dd>
              <dd v-else class="muted">não definida (os dados de fora precisam dela)</dd>
            </dl>
          </section>
        </div>

        <aside class="col">
          <div class="sticky">
            <ClientOnly>
              <OledPreview
                :page="pages[previewIndex] ?? pages[0]!"
                :index="previewIndex"
                :count="pages.length"
                :reading="latest?.latest ?? null"
                :today="latest?.last24h ?? null"
                :dim="cfg.display.contrast < 40"
              />
            </ClientOnly>
            <p class="muted small">Prévia da página {{ previewIndex + 1 }} com a última média; a fonte real do OLED é um pouco diferente.</p>

            <section class="block">
              <h3>Estado</h3>
              <dl class="kv">
                <dt>Identificação</dt><dd class="mono">{{ data.device.id }}</dd>
                <dt>Local</dt><dd>{{ data.device.place ? PLACE_LABEL[data.device.place] : '–' }}</dd>
                <dt>Firmware</dt><dd>{{ data.device.status?.fw ?? '–' }}</dd>
                <dt>Sinal Wi-Fi</dt><dd>{{ data.device.status?.rssi != null ? data.device.status.rssi + ' dBm' : '–' }}</dd>
                <dt>Ligada há</dt><dd>{{ uptime(data.device.status?.uptime_s) }}</dd>
                <dt>Fila no ESP32</dt><dd>{{ data.device.status?.queue ?? '–' }}</dd>
                <dt>Último envio</dt><dd>{{ when(data.device.last_seen_at) }}</dd>
                <dt>Cópia da configuração</dt><dd>{{ when(data.config_at) }}</dd>
              </dl>
            </section>
          </div>
        </aside>
      </div>
    </template>
  </main>
</template>

<style scoped>
.wrap { max-width: 1040px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 20px; }
.intro { display: grid; gap: 4px; }
.intro p { margin: 0; max-width: 70ch; }
h2 { font-size: 1.4rem; font-weight: 760; font-stretch: 110%; }
h3 { font-size: 1.05rem; font-weight: 700; font-stretch: 106%; }
.layout { display: grid; grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr); gap: 22px; align-items: start; }
@media (max-width: 820px) { .layout { grid-template-columns: 1fr; } }
.col { display: grid; gap: 16px; min-width: 0; align-content: start; }
.sticky { position: sticky; top: calc(env(safe-area-inset-top, 0px) + 12px); display: grid; gap: 12px; }
.block { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 10px; min-width: 0; }
.block-head { display: flex; justify-content: space-between; align-items: center; gap: 10px; }
.small { font-size: 0.85rem; margin: 0; }
.pages { list-style: none; margin: 0; padding: 0; display: grid; gap: 6px; }
.pages li { display: grid; grid-template-columns: 32px minmax(0, 1fr); gap: 10px; align-items: center; padding: 6px 8px; border: 1px solid var(--line); border-radius: 10px; background: var(--bg); }
.pages li.active { border-color: var(--accent); box-shadow: inset 3px 0 0 var(--accent); }
.num { width: 32px; height: 32px; border-radius: 8px; border: 1px solid var(--line); background: var(--surface); color: var(--fg); font-family: var(--ff-mono); font-weight: 600; cursor: pointer; }
.num:hover { border-color: var(--accent); }
.kv { display: grid; grid-template-columns: auto 1fr; margin: 0; }
.kv dt, .kv dd { margin: 0; padding: 7px 0; border-bottom: 1px solid var(--line); }
.kv dt { color: var(--muted); font-size: 0.9rem; padding-right: 12px; }
.kv dd { text-align: right; font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
.kv dt:last-of-type, .kv dd:last-of-type { border-bottom: none; }
.notice { background: var(--surface); border-left: 3px solid var(--accent); padding: 12px 16px; border-radius: 0 8px 8px 0; display: grid; gap: 4px; margin: 0; }
.notice span { font-size: 0.92rem; color: var(--muted); }
.notice.crit { border-left-color: var(--crit); }
code { font-family: var(--ff-mono); font-size: 0.88em; background: var(--sunk); padding: 0.05em 0.35em; border-radius: 4px; }
</style>
