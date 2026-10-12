<script setup lang="ts">
useHead({ title: 'Display · GWeather' })

type ConfigResp = {
  device_id: string
  version: number
  config: StationConfig
  is_default: boolean
  updated_at: string | null
  applied_version: number | null
  last_poll_at: string | null
}
type Latest = {
  device: { last_seen_at: string | null } | null
  latest: { temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null } | null
  last24h: { temp_min: number | null; temp_max: number | null; hum_min: number | null; hum_max: number | null } | null
}

const { data: remote, refresh } = await useFetch<ConfigResp>('/api/v1/config')
const { data: latest } = await useFetch<Latest>('/api/v1/latest')
const tz = useRuntimeConfig().public.tz as string

// Cópia editável; "dirty" compara com o que está salvo.
const form = ref<StationConfig>(structuredClone(toRaw(remote.value?.config ?? DEFAULT_CONFIG)))
const saved = ref(JSON.stringify(remote.value?.config ?? DEFAULT_CONFIG))
const dirty = computed(() => JSON.stringify(form.value) !== saved.value)

// --- páginas ---
const pages = computed(() => form.value.display.pages)
const previewIndex = ref(0)
const newType = ref<PageType>('clock')

function move(i: number, d: number) {
  const p = pages.value
  const j = i + d
  if (j < 0 || j >= p.length) return
  ;[p[i], p[j]] = [p[j]!, p[i]!]
  previewIndex.value = j
}
function remove(i: number) {
  if (pages.value.length <= 1) return
  pages.value.splice(i, 1)
  previewIndex.value = Math.min(previewIndex.value, pages.value.length - 1)
}
function addPage() {
  if (pages.value.length >= MAX_PAGES) return
  const p: PageConfig = newType.value === 'fields' ? { type: 'fields', fields: ['temp', 'hum', 'pres', 'dew'] } : { type: newType.value }
  pages.value.push(p)
  previewIndex.value = pages.value.length - 1
}
function changeType(p: PageConfig, t: PageType) {
  p.type = t
  if (t === 'fields' && !p.fields?.length) p.fields = ['temp', 'hum', 'pres', 'dew']
  if (t !== 'fields') delete p.fields
}
function setField(p: PageConfig, k: number, v: string) {
  const fs = [...(p.fields ?? [])]
  if (v === '') fs.splice(k, 1)
  else fs[k] = v as FieldType
  p.fields = fs.filter(Boolean)
}

// Prévia gira sozinha no ritmo configurado; clicar numa página fixa nela por um tempo.
let spin: ReturnType<typeof setInterval> | undefined
let pausedUntil = 0
function startSpin() {
  clearInterval(spin)
  spin = setInterval(() => {
    if (Date.now() < pausedUntil) return
    previewIndex.value = (previewIndex.value + 1) % pages.value.length
  }, form.value.display.page_s * 1000)
}
function preview(i: number) {
  previewIndex.value = i
  pausedUntil = Date.now() + 15_000
}
onMounted(startSpin)
onBeforeUnmount(() => clearInterval(spin))
watch(() => form.value.display.page_s, startSpin)

const brightnessPct = computed(() => Math.round((form.value.display.contrast / 255) * 100))

// --- salvar e acompanhar a aplicação na estação ---
const saving = ref(false)
const error = ref('')
const savedVersion = ref<number | null>(null)
let follow: ReturnType<typeof setInterval> | undefined

async function save() {
  error.value = ''
  saving.value = true
  try {
    const r = await $fetch<{ version: number; config: StationConfig }>('/api/v1/config', { method: 'PUT', body: form.value })
    form.value = r.config
    saved.value = JSON.stringify(r.config)
    savedVersion.value = r.version
    await refresh()
    clearInterval(follow)
    follow = setInterval(async () => {
      await refresh()
      if ((remote.value?.applied_version ?? 0) >= r.version) clearInterval(follow)
    }, 5000)
  } catch (e: any) {
    error.value = e?.data?.message || 'Não foi possível salvar.'
  } finally {
    saving.value = false
  }
}
onBeforeUnmount(() => clearInterval(follow))

function discard() {
  form.value = JSON.parse(saved.value)
  previewIndex.value = 0
}
function restoreDefaults() {
  form.value = structuredClone(DEFAULT_CONFIG)
  previewIndex.value = 0
}

const applyState = computed(() => {
  const r = remote.value
  if (!r || r.is_default) return { kind: 'idle', text: 'Usando a configuração padrão. Salve para enviar à estação.' }
  if ((r.applied_version ?? 0) >= r.version) {
    const when = r.last_poll_at ? new Date(r.last_poll_at).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: tz }) : ''
    return { kind: 'ok', text: `Versão ${r.version} aplicada na estação${when ? ` (confirmado às ${when})` : ''}.` }
  }
  return { kind: 'wait', text: `Versão ${r.version} salva. Aguardando a estação buscar (até 1 min).` }
})

const today = computed(() => latest.value?.last24h ?? null)
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="latest?.device?.last_seen_at ?? null" />

    <div class="intro">
      <h2>Display da estação</h2>
      <p class="muted">Escolha as páginas, a ordem e o comportamento do OLED. Ao salvar, a estação aplica sozinha em até 1 minuto.</p>
    </div>

    <div class="layout">
      <div class="editor">
        <section class="block">
          <div class="block-head">
            <h3>Páginas</h3>
            <span class="muted small">{{ pages.length }} de {{ MAX_PAGES }}</span>
          </div>
          <ol class="pages">
            <li v-for="(p, i) in pages" :key="i" :class="{ active: i === previewIndex }">
              <button type="button" class="num" :aria-label="`Ver prévia da página ${i + 1}`" @click="preview(i)">{{ i + 1 }}</button>
              <div class="page-main">
                <select :id="`type-${i}`" :value="p.type" :aria-label="`Tipo da página ${i + 1}`" @change="changeType(p, ($event.target as HTMLSelectElement).value as PageType)">
                  <option v-for="t in PAGE_TYPES" :key="t" :value="t">{{ PAGE_LABELS[t] }}</option>
                </select>
                <div v-if="p.type === 'fields'" class="fields">
                  <select
                    v-for="k in MAX_FIELDS"
                    :id="`field-${i}-${k}`"
                    :key="k"
                    :value="p.fields?.[k - 1] ?? ''"
                    :aria-label="`Campo ${k} da página ${i + 1}`"
                    @change="setField(p, k - 1, ($event.target as HTMLSelectElement).value)"
                  >
                    <option value="" :disabled="k === 1">—</option>
                    <option v-for="f in FIELD_TYPES" :key="f" :value="f">{{ FIELD_LABELS[f] }}</option>
                  </select>
                </div>
              </div>
              <div class="tools">
                <button type="button" class="icon" :disabled="i === 0" aria-label="Subir" @click="move(i, -1)">↑</button>
                <button type="button" class="icon" :disabled="i === pages.length - 1" aria-label="Descer" @click="move(i, 1)">↓</button>
                <button type="button" class="icon" :disabled="pages.length <= 1" aria-label="Remover" @click="remove(i)">✕</button>
              </div>
            </li>
          </ol>
          <div class="add">
            <select id="new-page" v-model="newType" aria-label="Tipo da nova página">
              <option v-for="t in PAGE_TYPES" :key="t" :value="t">{{ PAGE_LABELS[t] }}</option>
            </select>
            <button type="button" class="btn" :disabled="pages.length >= MAX_PAGES" @click="addPage">Adicionar página</button>
          </div>
        </section>

        <section class="block grid2">
          <div class="field">
            <label for="page-s">Tempo por página</label>
            <div class="inline">
              <input id="page-s" v-model.number="form.display.page_s" type="range" min="2" max="60" step="1">
              <span class="val">{{ form.display.page_s }} s</span>
            </div>
          </div>
          <div class="field">
            <label for="contrast">Brilho</label>
            <div class="inline">
              <input id="contrast" v-model.number="form.display.contrast" type="range" min="0" max="255" step="5">
              <span class="val">{{ brightnessPct }}%</span>
            </div>
          </div>
        </section>

        <section class="block">
          <div class="block-head">
            <h3>Modo noturno</h3>
            <label class="switch" for="night-on">
              <input id="night-on" v-model="form.display.night.enabled" type="checkbox">
              <span>{{ form.display.night.enabled ? 'Ligado' : 'Desligado' }}</span>
            </label>
          </div>
          <div class="night" :class="{ off: !form.display.night.enabled }">
            <div class="field">
              <label for="night-from">Das</label>
              <input id="night-from" v-model="form.display.night.from" type="time" :disabled="!form.display.night.enabled">
            </div>
            <div class="field">
              <label for="night-to">às</label>
              <input id="night-to" v-model="form.display.night.to" type="time" :disabled="!form.display.night.enabled">
            </div>
            <fieldset class="field" :disabled="!form.display.night.enabled">
              <legend>Durante a noite</legend>
              <label for="mode-dim"><input id="mode-dim" v-model="form.display.night.mode" type="radio" value="dim"> escurecer</label>
              <label for="mode-off"><input id="mode-off" v-model="form.display.night.mode" type="radio" value="off"> apagar</label>
            </fieldset>
          </div>
        </section>

        <section class="block">
          <h3>Calibração</h3>
          <p class="muted small">Valor somado a cada leitura. Use para alinhar com um termômetro ou higrômetro de referência, ou com a pressão do aeroporto mais próximo.</p>
          <div class="grid3">
            <div class="field">
              <label for="cal-temp">Temperatura (°C)</label>
              <input id="cal-temp" v-model.number="form.calibration.temp" type="number" min="-5" max="5" step="0.1">
            </div>
            <div class="field">
              <label for="cal-hum">Umidade (%)</label>
              <input id="cal-hum" v-model.number="form.calibration.hum" type="number" min="-15" max="15" step="0.5">
            </div>
            <div class="field">
              <label for="cal-pres">Pressão (hPa)</label>
              <input id="cal-pres" v-model.number="form.calibration.pres" type="number" min="-15" max="15" step="0.1">
            </div>
          </div>
        </section>
      </div>

      <aside class="side">
        <div class="sticky">
          <OledPreview
            :page="pages[previewIndex] ?? pages[0]!"
            :index="previewIndex"
            :count="pages.length"
            :reading="latest?.latest ?? null"
            :today="today"
            :dim="form.display.contrast < 40"
          />
          <p class="muted small cap">
            Prévia da página {{ previewIndex + 1 }}: {{ PAGE_LABELS[(pages[previewIndex] ?? pages[0]!).type] }}. Valores da última média; a fonte real do OLED é um pouco diferente.
          </p>

          <div class="savebar">
            <p v-if="error" class="msg crit" role="alert">{{ error }}</p>
            <p v-else-if="dirty" class="msg warn">Alterações não salvas.</p>
            <p v-else class="msg" :class="applyState.kind">{{ applyState.text }}</p>
            <div class="actions">
              <button type="button" class="btn primary" :disabled="!dirty || saving" @click="save">{{ saving ? 'Salvando…' : 'Salvar e enviar' }}</button>
              <button type="button" class="btn" :disabled="!dirty" @click="discard">Descartar</button>
              <button type="button" class="btn subtle" @click="restoreDefaults">Padrão</button>
            </div>
          </div>
        </div>
      </aside>
    </div>
  </main>
</template>

<style scoped>
.wrap { max-width: 1040px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 20px; }
.intro { display: grid; gap: 4px; }
h2 { font-size: 1.4rem; font-weight: 760; font-stretch: 110%; }
h3 { font-size: 1.05rem; font-weight: 700; font-stretch: 106%; }
.intro p { margin: 0; }
.layout { display: grid; grid-template-columns: minmax(0, 1.25fr) minmax(0, 1fr); gap: 22px; align-items: start; }
@media (max-width: 820px) { .layout { grid-template-columns: 1fr; } .side { order: -1; } }
.editor { display: grid; gap: 16px; min-width: 0; }
.block { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 12px; min-width: 0; }
.block-head { display: flex; justify-content: space-between; align-items: center; gap: 10px; }
.small { font-size: 0.85rem; margin: 0; }
.pages { list-style: none; margin: 0; padding: 0; display: grid; gap: 8px; }
.pages li { display: grid; grid-template-columns: 32px minmax(0, 1fr) auto; gap: 10px; align-items: start; padding: 8px; border: 1px solid var(--line); border-radius: 10px; background: var(--bg); }
.pages li.active { border-color: var(--accent); box-shadow: inset 3px 0 0 var(--accent); }
.num { width: 32px; height: 32px; border-radius: 8px; border: 1px solid var(--line); background: var(--surface); color: var(--fg); font-family: var(--ff-mono); font-weight: 600; cursor: pointer; }
.page-main { display: grid; gap: 6px; min-width: 0; }
.fields { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 6px; }
select, input[type='time'], input[type='number'] { font: inherit; font-size: 0.92rem; padding: 6px 8px; border: 1px solid var(--line); border-radius: 8px; background: var(--surface); color: var(--fg); min-width: 0; width: 100%; }
.tools { display: flex; gap: 4px; }
.icon { width: 32px; height: 32px; border-radius: 8px; border: 1px solid var(--line); background: var(--surface); color: var(--fg); cursor: pointer; font-size: 0.95rem; }
.icon:disabled { opacity: 0.35; cursor: default; }
.icon:hover:not(:disabled), .num:hover { border-color: var(--accent); }
.add { display: flex; gap: 8px; flex-wrap: wrap; }
.add select { width: auto; flex: 1; }
.grid2 { grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
.grid3 { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; }
.field { display: grid; gap: 4px; min-width: 0; border: 0; padding: 0; margin: 0; }
.field label, legend { font-size: 0.82rem; color: var(--muted); padding: 0; }
fieldset.field label { color: var(--fg); font-size: 0.92rem; display: inline-flex; align-items: center; gap: 6px; margin-right: 12px; }
.inline { display: flex; align-items: center; gap: 10px; }
.inline input[type='range'] { flex: 1; accent-color: var(--accent); }
.val { font-family: var(--ff-mono); font-size: 0.85rem; min-width: 3.5em; text-align: right; }
.night { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 10px; align-items: end; }
.night.off { opacity: 0.55; }
.switch { display: inline-flex; align-items: center; gap: 8px; font-size: 0.9rem; cursor: pointer; }
.switch input { width: 18px; height: 18px; accent-color: var(--accent); }
.side { min-width: 0; }
.sticky { position: sticky; top: calc(env(safe-area-inset-top, 0px) + 12px); display: grid; gap: 12px; }
.cap { line-height: 1.4; }
.savebar { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 10px; }
.msg { margin: 0; font-size: 0.9rem; padding-left: 10px; border-left: 3px solid var(--line); }
.msg.ok { border-color: var(--ok); }
.msg.wait, .msg.warn { border-color: var(--warn); }
.msg.crit { border-color: var(--crit); color: var(--crit); }
.actions { display: flex; flex-wrap: wrap; gap: 8px; }
.btn:disabled { opacity: 0.45; cursor: default; }
.btn.subtle { margin-left: auto; color: var(--muted); }
</style>
