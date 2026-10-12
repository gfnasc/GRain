// Textos das mensagens de WhatsApp.

type Row = { ts: Date; temp: number | null; hum: number | null; pres_slp: number | null }

const n1 = (v: number | null | undefined) => (v == null ? '–' : v.toLocaleString('pt-BR', { minimumFractionDigits: 1, maximumFractionDigits: 1 }))
const n0 = (v: number | null | undefined) => (v == null ? '–' : v.toLocaleString('pt-BR', { maximumFractionDigits: 0, useGrouping: false }))

export function localParts(date: Date, tz: string) {
  const f = (o: Intl.DateTimeFormatOptions) => date.toLocaleString('pt-BR', { ...o, timeZone: tz })
  return {
    hhmm: f({ hour: '2-digit', minute: '2-digit' }),
    day: f({ weekday: 'short', day: '2-digit', month: '2-digit' }),
    iso: date.toLocaleDateString('sv-SE', { timeZone: tz }), // AAAA-MM-DD
  }
}

async function latestRow(deviceId: string): Promise<Row | null> {
  const sql = useSql()
  const r = (await sql`select ts, temp, hum, pres_slp from readings where device_id = ${deviceId} order by ts desc limit 1`) as any[]
  return r[0] ? { ...r[0], ts: new Date(r[0].ts) } : null
}

// Variação da pressão nas últimas 3 h (média mais recente menos a de ~3 h antes).
export async function pressureDelta3h(deviceId: string, latest: Row | null): Promise<number | null> {
  if (!latest?.pres_slp) return null
  const sql = useSql()
  const past = (await sql`
    select pres_slp from readings
    where device_id = ${deviceId} and pres_slp is not null
      and ts between ${new Date(latest.ts.getTime() - 3.5 * 3600_000).toISOString()} and ${new Date(latest.ts.getTime() - 2.5 * 3600_000).toISOString()}
    order by abs(extract(epoch from ts) - ${latest.ts.getTime() / 1000 - 3 * 3600}) limit 1`) as any[]
  return past[0] ? latest.pres_slp - past[0].pres_slp : null
}

async function todayStats(deviceId: string, tz: string) {
  const sql = useSql()
  const r = (await sql`
    with d as (
      select ts, temp, hum, pres_slp from readings
      where device_id = ${deviceId} and (ts at time zone ${tz})::date = (now() at time zone ${tz})::date
    )
    select
      (select json_build_object('v', temp, 'ts', ts) from d where temp is not null order by temp desc limit 1) as tmax,
      (select json_build_object('v', temp, 'ts', ts) from d where temp is not null order by temp asc limit 1) as tmin,
      (select min(hum) from d) as hmin, (select max(hum) from d) as hmax,
      (select avg(pres_slp) from d) as pavg, (select count(*)::int from d) as n`) as any[]
  return r[0]
}

function trendLine(delta: number | null, pres: number | null): string | null {
  const trend = trendOf(delta)
  if (!trend || pres == null) return null
  const what = zambretti(pres, trend)
  if (trend === 'estável') return `Pressão estável: ${what}`
  const verb = trend === 'subindo' ? 'subiu' : 'caiu'
  return `Pressão ${verb} ${n1(Math.abs(delta!))} hPa em 3 h: ${what}`
}

function forecastLine(prefix: string, f: DayForecast | undefined): string | null {
  if (!f) return null
  const parts = [`máx ${n0(f.tmax)} °C`, `mín ${n0(f.tmin)} °C`]
  if (f.rain != null) parts.push(`chuva ${n0(f.rain)} %`)
  return `${prefix}: ${parts.join(', ')}${f.text ? ` · ${f.text}` : ''}`
}

// Monta a mensagem do tipo pedido com os dados atuais.
export async function composeMessage(type: MessageType, deviceId: string, cfg: NotifyConfig, tz: string): Promise<string> {
  const now = new Date()
  const p = localParts(now, tz)
  const latest = await latestRow(deviceId)
  const stale = !latest || now.getTime() - latest.ts.getTime() > 30 * 60_000
  const delta = await pressureDelta3h(deviceId, latest)
  const loc = cfg.location
  const fc = loc.lat != null && loc.lon != null ? await openMeteo(loc.lat, loc.lon, tz) : null
  const hi = latest?.temp != null && latest?.hum != null ? heatIndexC(latest.temp, latest.hum) : null

  const lines: (string | null)[] = []
  if (type === 'forecast') {
    lines.push(`🌤️ Bom dia! GWeather · ${p.day}`)
    lines.push(`Agora: ${n1(latest?.temp)} °C · ${n0(latest?.hum)} % · ${n0(latest?.pres_slp)} hPa`)
    lines.push(forecastLine('Hoje', fc?.[0]))
    lines.push(trendLine(delta, latest?.pres_slp ?? null))
  } else if (type === 'current') {
    lines.push(`☀️ GWeather · ${p.hhmm}`)
    lines.push(`${n1(latest?.temp)} °C${hi != null && Math.abs(hi - (latest?.temp ?? 0)) >= 1 ? ` · sensação ${n0(hi)} °C` : ''}`)
    const t = trendOf(delta)
    lines.push(`Umidade ${n0(latest?.hum)} % · ${n0(latest?.pres_slp)} hPa${t ? ` (${t})` : ''}`)
  } else if (type === 'trend') {
    lines.push(`🌇 GWeather · ${p.hhmm}`)
    lines.push(`${n1(latest?.temp)} °C · ${n0(latest?.hum)} %`)
    lines.push(trendLine(delta, latest?.pres_slp ?? null) ?? 'Tendência da pressão ainda indisponível (faltam 3 h de dados).')
    lines.push(forecastLine('Amanhã', fc?.[1]))
  } else {
    const s = await todayStats(deviceId, tz)
    lines.push(`📊 Resumo de ${p.day}`)
    if (s?.n) {
      const at = (x: any) => (x ? localParts(new Date(x.ts), tz).hhmm : '')
      lines.push(`Máx ${n1(s.tmax?.v)} °C às ${at(s.tmax)} · Mín ${n1(s.tmin?.v)} °C às ${at(s.tmin)}`)
      lines.push(`Umidade ${n0(s.hmin)}–${n0(s.hmax)} % · Pressão média ${n0(s.pavg)} hPa`)
    } else {
      lines.push('Sem dados da estação hoje.')
    }
    lines.push(forecastLine('Amanhã', fc?.[1]))
  }
  if (stale) lines.push('⚠️ Sem dados recentes da estação.')
  return lines.filter(Boolean).join('\n')
}

// Avalia os alertas com a média mais recente. Devolve [tipo, texto] dos que dispararam.
export async function evaluateAlerts(deviceId: string, cfg: NotifyConfig, tz: string): Promise<[string, string, number][]> {
  const a = cfg.alerts
  const latest = await latestRow(deviceId)
  if (!latest || Date.now() - latest.ts.getTime() > 30 * 60_000) return []
  const when = localParts(latest.ts, tz).hhmm
  const out: [string, string, number][] = []
  const ts = Math.floor(latest.ts.getTime() / 1000)
  if (a.temp_max != null && latest.temp != null && latest.temp >= a.temp_max) {
    out.push(['temp_max', `🔥 Alerta de calor: ${n1(latest.temp)} °C às ${when} (limite ${n0(a.temp_max)} °C)`, ts])
  }
  if (a.temp_min != null && latest.temp != null && latest.temp <= a.temp_min) {
    out.push(['temp_min', `🥶 Alerta de frio: ${n1(latest.temp)} °C às ${when} (limite ${n0(a.temp_min)} °C)`, ts])
  }
  if (a.pres_drop_3h != null) {
    const d = await pressureDelta3h(deviceId, latest)
    if (d != null && d <= -a.pres_drop_3h) {
      out.push(['pres_drop', `🌧️ Pressão caiu ${n1(-d)} hPa em 3 h (até ${when}): possível mudança de tempo, com chance de chuva ou vento.`, ts])
    }
  }
  return out
}
