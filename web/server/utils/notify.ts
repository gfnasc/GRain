// Envio de notificações por WhatsApp (CallMeBot) com registro sem duplicatas.

type Recipient = { id: number; name: string; phone: string; apikey: string; enabled: boolean }

export async function loadNotify(): Promise<{ deviceId: string | null; cfg: NotifyConfig }> {
  const sql = useSql()
  const r = (await sql`select id, notify from devices order by created_at limit 1`) as any[]
  return { deviceId: r[0]?.id ?? null, cfg: (r[0]?.notify as NotifyConfig | null) ?? DEFAULT_NOTIFY }
}

// Envia para um número. CallMeBot responde 200 com HTML; erro vem no texto.
export async function sendWhatsApp(phone: string, apikey: string, text: string): Promise<{ ok: boolean; detail: string }> {
  const url = new URL('https://api.callmebot.com/whatsapp.php')
  url.search = new URLSearchParams({ phone, text, apikey }).toString()
  try {
    const r = await fetch(url, { signal: AbortSignal.timeout(10_000) })
    const body = (await r.text()).replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 160)
    const failed = !r.ok || /error|invalid|not allowed|paused/i.test(body)
    return { ok: !failed, detail: failed ? `HTTP ${r.status}: ${body}` : 'ok' }
  } catch (e) {
    return { ok: false, detail: (e as Error).message }
  }
}

async function recipients(only?: number): Promise<Recipient[]> {
  const sql = useSql()
  const rows = (await sql`select id, name, phone, apikey, enabled from recipients order by id`) as Recipient[]
  return rows.filter((r) => (only ? r.id === only : r.enabled))
}

// Reserva o "slot" (insere no registro). Se já existe, outro envio já cuidou dele.
async function claim(slot: string, kind: string, type: string): Promise<boolean> {
  const sql = useSql()
  const r = (await sql`insert into notifications (slot, kind, type) values (${slot}, ${kind}, ${type})
    on conflict (slot) do nothing returning id`) as any[]
  return r.length > 0
}

async function finish(slot: string, status: string, detail: string, message: string) {
  const sql = useSql()
  await sql`update notifications set status = ${status}, detail = ${detail}, message = ${message} where slot = ${slot}`
}

// Envia para os destinatários (ou só um) e registra o resultado.
export async function deliver(slot: string, kind: string, type: string, text: string, only?: number) {
  if (!(await claim(slot, kind, type))) return { status: 'duplicado', detail: 'já enviado' }
  const list = await recipients(only)
  if (!list.length) {
    await finish(slot, 'sem_destinatarios', 'nenhum destinatário ativo', text)
    return { status: 'sem_destinatarios', detail: 'nenhum destinatário ativo' }
  }
  const results: string[] = []
  let ok = 0
  for (const r of list) {
    const res = await sendWhatsApp(r.phone, r.apikey, text)
    if (res.ok) ok++
    results.push(`${r.name}: ${res.ok ? 'ok' : res.detail}`)
  }
  const status = ok === list.length ? 'enviado' : ok ? 'parcial' : 'falhou'
  await finish(slot, status, results.join(' · '), text)
  return { status, detail: results.join(' · ') }
}

// Chamado a cada consulta da estação (~1 por minuto): dispara a agenda.
// Envia no horário ou até 30 min depois (se a estação estava fora do ar).
export async function runSchedules(deviceId: string, cfg: NotifyConfig, tz: string) {
  if (!cfg.enabled) return
  const now = new Date()
  const p = localParts(now, tz)
  const [h, m] = p.hhmm.split(':').map(Number)
  const nowMin = h! * 60 + m!
  const weekday = new Date(now.toLocaleString('en-US', { timeZone: tz })).getDay()
  for (const s of cfg.schedules) {
    if (!s.enabled || !s.days.includes(weekday)) continue
    const [sh, sm] = s.time.split(':').map(Number)
    const at = sh! * 60 + sm!
    if (nowMin < at || nowMin >= at + 30) continue
    const slot = `agenda:${s.id}:${p.iso}`
    // Checagem barata antes de montar a mensagem.
    const sql = useSql()
    const exists = (await sql`select 1 from notifications where slot = ${slot}`) as any[]
    if (exists.length) continue
    const text = await composeMessage(s.type, deviceId, cfg, tz)
    await deliver(slot, 'schedule', s.type, text)
  }
}

// Chamado depois de cada lote recebido (~10 min): avalia os alertas.
export async function runAlerts(deviceId: string, cfg: NotifyConfig, tz: string) {
  if (!cfg.enabled || !cfg.alerts.enabled) return
  const fired = await evaluateAlerts(deviceId, cfg, tz)
  const sql = useSql()
  for (const [type, text, ts] of fired) {
    const last = (await sql`select created_at from notifications
      where kind = 'alert' and type = ${type} and status in ('enviado', 'parcial')
      order by created_at desc limit 1`) as any[]
    if (last[0] && Date.now() - new Date(last[0].created_at).getTime() < cfg.alerts.min_gap_h * 3600_000) continue
    await deliver(`alerta:${type}:${ts}`, 'alert', type, text)
  }
}
