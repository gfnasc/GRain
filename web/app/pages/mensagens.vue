<script setup lang="ts">
useHead({ title: 'Mensagens · GWeather' })

type RecipientRow = { id: number; name: string; phone: string; enabled: boolean; has_key: boolean }
type LogRow = { slot: string; kind: string; type: string; status: string; detail: string | null; message: string | null; created_at: string }
type NotifyResp = { config: NotifyConfig; recipients: RecipientRow[]; log: LogRow[] }

const { data, refresh } = await useFetch<NotifyResp>('/api/v1/notify')
const { data: latest } = await useFetch<{ device: { last_seen_at: string | null } | null }>('/api/v1/latest')
const tz = useRuntimeConfig().public.tz as string

// --- configuração (agenda, alertas, local) ---
const form = ref<NotifyConfig>(structuredClone(toRaw(data.value?.config ?? DEFAULT_NOTIFY)))
const saved = ref(JSON.stringify(data.value?.config ?? DEFAULT_NOTIFY))
const dirty = computed(() => JSON.stringify(form.value) !== saved.value)
const saving = ref(false)
const cfgMsg = ref<{ kind: string; text: string } | null>(null)

const DAYS = ['D', 'S', 'T', 'Q', 'Q', 'S', 'S']
const DAY_NAMES = ['domingo', 'segunda', 'terça', 'quarta', 'quinta', 'sexta', 'sábado']

function toggleDay(s: Schedule, d: number) {
  s.days = s.days.includes(d) ? s.days.filter((x) => x !== d) : [...s.days, d].sort()
}
function addSchedule() {
  if (form.value.schedules.length >= MAX_SCHEDULES) return
  form.value.schedules.push({ id: `h-${Date.now().toString(36)}`, time: '08:00', type: 'current', days: [0, 1, 2, 3, 4, 5, 6], enabled: true })
}
function removeSchedule(i: number) {
  form.value.schedules.splice(i, 1)
}

async function saveConfig() {
  saving.value = true
  cfgMsg.value = null
  try {
    const r = await $fetch<{ config: NotifyConfig }>('/api/v1/notify', { method: 'PUT', body: form.value })
    form.value = r.config
    saved.value = JSON.stringify(r.config)
    cfgMsg.value = { kind: 'ok', text: 'Salvo. A agenda passa a valer na próxima consulta da estação (até 1 min).' }
  } catch (e: any) {
    cfgMsg.value = { kind: 'crit', text: e?.data?.message || 'Não foi possível salvar.' }
  } finally {
    saving.value = false
  }
}

const geoMsg = ref('')
function useMyLocation() {
  geoMsg.value = 'Obtendo localização…'
  if (!navigator.geolocation) {
    geoMsg.value = 'Este navegador não informa a localização. Digite latitude e longitude.'
    return
  }
  navigator.geolocation.getCurrentPosition(
    (p) => {
      form.value.location.lat = Math.round(p.coords.latitude * 100) / 100
      form.value.location.lon = Math.round(p.coords.longitude * 100) / 100
      geoMsg.value = 'Localização preenchida (arredondada a ~1 km). Salve para usar.'
    },
    () => (geoMsg.value = 'Sem permissão de localização. Digite latitude e longitude.'),
    { timeout: 10_000 },
  )
}

// --- prévia ---
const previewType = ref<MessageType>('forecast')
const previewText = ref('')
const previewBusy = ref(false)
async function loadPreview() {
  previewBusy.value = true
  try {
    previewText.value = (await $fetch<{ text: string }>('/api/v1/notify/preview', { query: { type: previewType.value } })).text
  } catch (e: any) {
    previewText.value = e?.data?.message || 'Não foi possível montar a prévia.'
  } finally {
    previewBusy.value = false
  }
}
onMounted(loadPreview)
watch(previewType, loadPreview)

// --- destinatários ---
const recipients = computed(() => data.value?.recipients ?? [])
const nr = ref({ name: '', phone: '', apikey: '' })
const recMsg = ref<{ kind: string; text: string } | null>(null)
const busyId = ref<number | null>(null)

async function addRecipient() {
  recMsg.value = null
  try {
    await $fetch('/api/v1/notify/recipients', { method: 'POST', body: nr.value })
    nr.value = { name: '', phone: '', apikey: '' }
    recMsg.value = { kind: 'ok', text: 'Destinatário cadastrado. Use "Enviar teste" para conferir.' }
    await refresh()
  } catch (e: any) {
    recMsg.value = { kind: 'crit', text: e?.data?.message || 'Não foi possível cadastrar.' }
  }
}
async function toggleRecipient(r: RecipientRow) {
  await $fetch(`/api/v1/notify/recipients/${r.id}`, { method: 'PATCH', body: { enabled: !r.enabled } })
  await refresh()
}
const confirmDelete = ref<number | null>(null)
async function deleteRecipient(r: RecipientRow) {
  if (confirmDelete.value !== r.id) {
    confirmDelete.value = r.id
    return
  }
  await $fetch(`/api/v1/notify/recipients/${r.id}`, { method: 'DELETE' })
  confirmDelete.value = null
  await refresh()
}
const newKey = ref<Record<number, string>>({})
async function replaceKey(r: RecipientRow) {
  const k = newKey.value[r.id]
  if (!k) return
  try {
    await $fetch(`/api/v1/notify/recipients/${r.id}`, { method: 'PATCH', body: { apikey: k } })
    newKey.value[r.id] = ''
    recMsg.value = { kind: 'ok', text: `Chave de ${r.name} trocada.` }
  } catch (e: any) {
    recMsg.value = { kind: 'crit', text: e?.data?.message || 'Não foi possível trocar a chave.' }
  }
}
async function sendTest(r?: RecipientRow) {
  busyId.value = r?.id ?? 0
  recMsg.value = null
  try {
    const res = await $fetch<{ status: string; detail: string }>('/api/v1/notify/test', {
      method: 'POST',
      body: { type: previewType.value, recipient_id: r?.id },
    })
    recMsg.value = {
      kind: res.status === 'enviado' ? 'ok' : 'crit',
      text: res.status === 'enviado' ? `Teste enviado (${MESSAGE_LABELS[previewType.value]}). Confira o WhatsApp.` : `Falhou: ${res.detail}`,
    }
    await refresh()
  } catch (e: any) {
    recMsg.value = { kind: 'crit', text: e?.data?.message || 'Não foi possível enviar.' }
  } finally {
    busyId.value = null
  }
}

// --- registro ---
const STATUS: Record<string, { label: string; kind: string }> = {
  enviado: { label: 'Enviado', kind: 'ok' },
  parcial: { label: 'Parcial', kind: 'warn' },
  falhou: { label: 'Falhou', kind: 'crit' },
  sem_destinatarios: { label: 'Sem destinatários', kind: 'warn' },
  pendente: { label: 'Enviando', kind: 'warn' },
}
const KIND: Record<string, string> = { schedule: 'Agenda', alert: 'Alerta', test: 'Teste' }
const ALERT_LABELS: Record<string, string> = { temp_max: 'Calor', temp_min: 'Frio', pres_drop: 'Queda de pressão' }
const when = (iso: string) =>
  new Date(iso).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', timeZone: tz })
const typeLabel = (l: LogRow) => (l.kind === 'alert' ? ALERT_LABELS[l.type] ?? l.type : MESSAGE_LABELS[l.type as MessageType] ?? l.type)
</script>

<template>
  <main class="wrap">
    <AppHeader :last-seen-at="latest?.device?.last_seen_at ?? null" />

    <div class="intro">
      <h2>Mensagens no WhatsApp</h2>
      <p class="muted">Enviadas pelo CallMeBot para cada destinatário ativo, nos horários da agenda e quando um alerta dispara. A estação precisa estar online: a agenda roda a cada consulta dela à nuvem (1 por minuto).</p>
    </div>

    <section class="block">
      <div class="head">
        <h3>Destinatários</h3>
        <span class="muted small">Cada pessoa ativa a própria chave no CallMeBot</span>
      </div>
      <ul v-if="recipients.length" class="recips">
        <li v-for="r in recipients" :key="r.id">
          <div class="who">
            <b>{{ r.name }}</b>
            <span class="mono muted">{{ r.phone }}</span>
          </div>
          <div class="row">
            <button type="button" class="btn" :class="{ on: r.enabled }" :aria-pressed="r.enabled" @click="toggleRecipient(r)">{{ r.enabled ? 'Ativo' : 'Pausado' }}</button>
            <button type="button" class="btn" :disabled="busyId !== null" @click="sendTest(r)">{{ busyId === r.id ? 'Enviando…' : 'Enviar teste' }}</button>
            <button type="button" class="btn subtle" @click="deleteRecipient(r)">{{ confirmDelete === r.id ? 'Confirmar remoção' : 'Remover' }}</button>
          </div>
          <form class="row key" @submit.prevent="replaceKey(r)">
            <label :for="`key-${r.id}`" class="muted small">Trocar chave</label>
            <input :id="`key-${r.id}`" v-model="newKey[r.id]" type="password" autocomplete="off" placeholder="nova apikey">
            <button type="submit" class="btn" :disabled="!newKey[r.id]">Salvar chave</button>
          </form>
        </li>
      </ul>
      <p v-else class="muted small">Nenhum destinatário ainda. Cadastre abaixo depois de ativar o CallMeBot no celular.</p>

      <form class="add" @submit.prevent="addRecipient">
        <div class="field"><label for="r-name">Nome</label><input id="r-name" v-model="nr.name" required maxlength="40" placeholder="Ex.: Maria"></div>
        <div class="field"><label for="r-phone">WhatsApp (com DDI e DDD)</label><input id="r-phone" v-model="nr.phone" required inputmode="tel" placeholder="+55 11 99999-9999"></div>
        <div class="field"><label for="r-key">Chave (apikey)</label><input id="r-key" v-model="nr.apikey" required type="password" autocomplete="off" placeholder="recebida do CallMeBot"></div>
        <button type="submit" class="btn primary">Cadastrar</button>
      </form>
      <details class="how">
        <summary>Como conseguir a chave do CallMeBot</summary>
        <ol>
          <li>Abra <a href="https://www.callmebot.com/blog/free-api-whatsapp-messages/" target="_blank" rel="noopener">a página da API de WhatsApp do CallMeBot</a> e salve o número do bot que aparece lá.</li>
          <li>Pelo WhatsApp, envie para ele a mensagem de autorização indicada na página.</li>
          <li>A resposta traz a sua <b>apikey</b>. Cadastre aqui com o seu número.</li>
        </ol>
      </details>
      <p v-if="recMsg" class="msg" :class="recMsg.kind" role="status">{{ recMsg.text }}</p>
    </section>

    <section class="block">
      <div class="head">
        <h3>Prévia</h3>
        <select id="preview-type" v-model="previewType" aria-label="Tipo de mensagem">
          <option v-for="t in MESSAGE_TYPES" :key="t" :value="t">{{ MESSAGE_LABELS[t] }}</option>
        </select>
      </div>
      <div class="bubble" :class="{ busy: previewBusy }">{{ previewText || '…' }}</div>
      <div class="row">
        <button type="button" class="btn" :disabled="busyId !== null || !recipients.some((r) => r.enabled)" @click="sendTest()">
          {{ busyId === 0 ? 'Enviando…' : 'Enviar esta prévia para todos os ativos' }}
        </button>
        <span class="muted small">Com os dados de agora.</span>
      </div>
    </section>

    <section class="block">
      <div class="head">
        <h3>Agenda</h3>
        <label class="switch" for="notify-on"><input id="notify-on" v-model="form.enabled" type="checkbox"> <span>{{ form.enabled ? 'Mensagens ligadas' : 'Mensagens desligadas' }}</span></label>
      </div>
      <ul class="sched">
        <li v-for="(s, i) in form.schedules" :key="s.id" :class="{ off: !s.enabled }">
          <input :id="`time-${s.id}`" v-model="s.time" type="time" :aria-label="`Horário ${i + 1}`">
          <select :id="`type-${s.id}`" v-model="s.type" :aria-label="`Tipo do horário ${i + 1}`">
            <option v-for="t in MESSAGE_TYPES" :key="t" :value="t">{{ MESSAGE_LABELS[t] }}</option>
          </select>
          <div class="days" role="group" :aria-label="`Dias do horário ${i + 1}`">
            <button v-for="(d, k) in DAYS" :key="k" type="button" class="day" :class="{ on: s.days.includes(k) }" :aria-pressed="s.days.includes(k)" :aria-label="DAY_NAMES[k]" @click="toggleDay(s, k)">{{ d }}</button>
          </div>
          <div class="row end">
            <button type="button" class="btn" :class="{ on: s.enabled }" :aria-pressed="s.enabled" @click="s.enabled = !s.enabled">{{ s.enabled ? 'Ativo' : 'Pausado' }}</button>
            <button type="button" class="icon" aria-label="Remover horário" @click="removeSchedule(i)">✕</button>
          </div>
        </li>
      </ul>
      <button type="button" class="btn" :disabled="form.schedules.length >= MAX_SCHEDULES" @click="addSchedule">Adicionar horário</button>
    </section>

    <section class="block">
      <div class="head">
        <h3>Alertas</h3>
        <label class="switch" for="alerts-on"><input id="alerts-on" v-model="form.alerts.enabled" type="checkbox"> <span>{{ form.alerts.enabled ? 'Ligados' : 'Desligados' }}</span></label>
      </div>
      <p class="muted small">Avaliados a cada média de 10 min. Deixe em branco para não usar um alerta.</p>
      <div class="grid4" :class="{ off: !form.alerts.enabled }">
        <div class="field"><label for="a-hot">Calor a partir de (°C)</label><input id="a-hot" v-model.number="form.alerts.temp_max" type="number" step="0.5"></div>
        <div class="field"><label for="a-cold">Frio abaixo de (°C)</label><input id="a-cold" v-model.number="form.alerts.temp_min" type="number" step="0.5"></div>
        <div class="field"><label for="a-pres">Queda de pressão em 3 h (hPa)</label><input id="a-pres" v-model.number="form.alerts.pres_drop_3h" type="number" step="0.5" min="0.5"></div>
        <div class="field"><label for="a-gap">Repetir no máximo a cada (h)</label><input id="a-gap" v-model.number="form.alerts.min_gap_h" type="number" step="1" min="1" max="24"></div>
      </div>
    </section>

    <section class="block">
      <h3>Local da previsão</h3>
      <p class="muted small">Usado para buscar máxima, mínima e chance de chuva na Open-Meteo. Fica só no banco do painel, não no código.</p>
      <div class="grid4">
        <div class="field"><label for="lat">Latitude</label><input id="lat" v-model.number="form.location.lat" type="number" step="0.01" placeholder="-23.55"></div>
        <div class="field"><label for="lon">Longitude</label><input id="lon" v-model.number="form.location.lon" type="number" step="0.01" placeholder="-46.63"></div>
        <div class="field"><label for="loclabel">Nome (opcional)</label><input id="loclabel" v-model="form.location.label" maxlength="40" placeholder="Minha cidade"></div>
        <div class="field"><span class="lbl">&nbsp;</span><button type="button" class="btn" @click="useMyLocation">Usar minha localização</button></div>
      </div>
      <p v-if="geoMsg" class="muted small">{{ geoMsg }}</p>
    </section>

    <div v-if="dirty || cfgMsg" class="savebar">
      <p v-if="cfgMsg" class="msg" :class="cfgMsg.kind">{{ cfgMsg.text }}</p>
      <p v-else-if="dirty" class="msg warn">Agenda, alertas ou local com alterações não salvas.</p>
      <button type="button" class="btn primary" :disabled="!dirty || saving" @click="saveConfig">{{ saving ? 'Salvando…' : 'Salvar agenda, alertas e local' }}</button>
    </div>

    <section class="block">
      <div class="head"><h3>Últimos envios</h3><button type="button" class="btn subtle" @click="refresh()">Atualizar</button></div>
      <ul v-if="data?.log.length" class="log">
        <li v-for="l in data.log" :key="l.slot">
          <div class="logline">
            <span class="mono muted">{{ when(l.created_at) }}</span>
            <span>{{ KIND[l.kind] ?? l.kind }} · {{ typeLabel(l) }}</span>
            <span class="pill" :class="STATUS[l.status]?.kind">{{ STATUS[l.status]?.label ?? l.status }}</span>
          </div>
          <details v-if="l.message || l.detail">
            <summary class="muted small">ver mensagem</summary>
            <div class="bubble small">{{ l.message }}</div>
            <p v-if="l.detail && l.status !== 'enviado'" class="muted small">{{ l.detail }}</p>
          </details>
        </li>
      </ul>
      <p v-else class="muted small">Nenhum envio ainda.</p>
    </section>
  </main>
</template>

<style scoped>
.wrap { max-width: 860px; margin: 0 auto; padding-inline: 16px; padding-block: 24px 56px; display: grid; gap: 18px; }
.intro { display: grid; gap: 4px; }
.intro p { margin: 0; }
h2 { font-size: 1.4rem; font-weight: 760; font-stretch: 110%; }
h3 { font-size: 1.05rem; font-weight: 700; font-stretch: 106%; }
.block { background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 14px 16px; display: grid; gap: 12px; min-width: 0; }
.head { display: flex; justify-content: space-between; align-items: center; gap: 10px; flex-wrap: wrap; }
.small { font-size: 0.85rem; margin: 0; }
.row { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
.row.end { justify-content: flex-end; }
input, select { font: inherit; font-size: 0.92rem; padding: 7px 9px; border: 1px solid var(--line); border-radius: 8px; background: var(--bg); color: var(--fg); min-width: 0; }
.field { display: grid; gap: 4px; min-width: 0; }
.field label, .lbl { font-size: 0.82rem; color: var(--muted); }
.btn.on { background: var(--fg); color: var(--bg); border-color: var(--fg); }
.btn:disabled { opacity: 0.45; cursor: default; }
.btn.subtle { color: var(--muted); }
.recips { list-style: none; margin: 0; padding: 0; display: grid; gap: 10px; }
.recips li { border: 1px solid var(--line); border-radius: 10px; padding: 10px 12px; display: grid; gap: 8px; background: var(--bg); }
.who { display: flex; gap: 10px; align-items: baseline; flex-wrap: wrap; }
.key input { flex: 1; min-width: 140px; }
.add { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 10px; align-items: end; }
.how summary { cursor: pointer; font-size: 0.9rem; }
.how ol { margin: 8px 0 0; padding-left: 1.2em; display: grid; gap: 4px; font-size: 0.9rem; }
.bubble { background: var(--sunk); border-radius: 10px 10px 10px 2px; padding: 10px 12px; white-space: pre-line; font-size: 0.92rem; line-height: 1.45; }
.bubble.busy { opacity: 0.6; }
.bubble.small { font-size: 0.85rem; margin-top: 6px; }
.sched { list-style: none; margin: 0; padding: 0; display: grid; gap: 8px; }
.sched li { display: grid; grid-template-columns: auto minmax(0, 1fr) auto auto; gap: 8px; align-items: center; border: 1px solid var(--line); border-radius: 10px; padding: 8px; background: var(--bg); }
.sched li.off { opacity: 0.6; }
@media (max-width: 640px) { .sched li { grid-template-columns: auto minmax(0, 1fr); } .days, .sched .row.end { grid-column: 1 / -1; } .sched .row.end { justify-content: flex-start; } }
.days { display: flex; gap: 3px; }
.day { width: 30px; height: 30px; border-radius: 50%; border: 1px solid var(--line); background: var(--surface); color: var(--muted); font-weight: 650; font-size: 0.8rem; cursor: pointer; }
.day.on { background: var(--accent); border-color: var(--accent); color: #fff; }
.icon { width: 32px; height: 32px; border-radius: 8px; border: 1px solid var(--line); background: var(--surface); color: var(--fg); cursor: pointer; }
.grid4 { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 10px; align-items: end; }
.grid4.off { opacity: 0.55; }
.switch { display: inline-flex; align-items: center; gap: 8px; font-size: 0.9rem; cursor: pointer; }
.switch input { width: 18px; height: 18px; accent-color: var(--accent); }
.savebar { position: sticky; bottom: calc(env(safe-area-inset-bottom, 0px) + 12px); background: var(--surface); border: 1px solid var(--line); border-radius: 12px; padding: 12px 14px; display: flex; gap: 12px; align-items: center; justify-content: space-between; flex-wrap: wrap; box-shadow: 0 6px 24px rgba(0, 0, 0, 0.12); z-index: 3; }
.savebar .msg { flex: 1; }
.msg { margin: 0; font-size: 0.9rem; padding-left: 10px; border-left: 3px solid var(--line); }
.msg.ok { border-color: var(--ok); }
.msg.warn { border-color: var(--warn); }
.msg.crit { border-color: var(--crit); color: var(--crit); }
.log { list-style: none; margin: 0; padding: 0; display: grid; gap: 6px; }
.log li { border-bottom: 1px solid var(--line); padding-bottom: 6px; display: grid; gap: 4px; }
.log li:last-child { border-bottom: none; }
.logline { display: grid; grid-template-columns: auto 1fr auto; gap: 10px; align-items: center; font-size: 0.9rem; }
.pill { font-family: var(--ff-mono); font-size: 0.72rem; padding: 2px 8px; border-radius: 999px; border: 1px solid var(--line); white-space: nowrap; }
.pill.ok { color: var(--ok); border-color: var(--ok); }
.pill.warn { color: var(--warn); border-color: var(--warn); }
.pill.crit { color: var(--crit); border-color: var(--crit); }
details summary { cursor: pointer; }
</style>
