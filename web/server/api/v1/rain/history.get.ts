// Probabilidade prevista por método ao longo do tempo (para uma janela de 1, 2 ou 3 h) e os
// trechos em que choveu (METAR, aviso no painel/estação ou, sem METAR, o modelo). Exige login.
//   ?range=24h|7d|30d (ou from/to)  &h=1|2|3

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const range = await resolveRange(event)
  const h = Math.min(3, Math.max(1, Number(getQuery(event).h ?? 1) || 1))
  const deviceId = await defaultDeviceId()
  if (!deviceId) return { from: range.from, to: range.to, h, points: [], rain: [] }
  const sql = useSql()
  const from = range.from.toISOString()
  const to = range.to.toISOString()

  const rows = (await sql`
    select extract(epoch from issued_at)::bigint as ts,
      max(prob) filter (where method = 'm1') as m1, max(prob) filter (where method = 'm2') as m2, max(prob) filter (where method = 'm3') as m3
    from rain_predictions where device_id = ${deviceId} and horizon_h = ${h} and issued_at >= ${from} and issued_at < ${to}
    group by 1 order by 1`) as { ts: string; m1: number | null; m2: number | null; m3: number | null }[]

  // Trechos de chuva: METAR com chuva (±30 min da observação), avisos (10 min) e modelo (hora anterior).
  const marks = (await sql`
    select extract(epoch from obs_at)::bigint - 1800 as a, extract(epoch from obs_at)::bigint + 1800 as b, 'aeroporto' as src
      from outdoor_obs o where device_id = ${deviceId} and source = 'metar' and obs_at >= ${from} and obs_at < ${to} and wx ~ ${METAR_RAIN_SQL}
        and not exists (select 1 from rain_reports n where n.device_id = o.device_id and not n.raining and abs(extract(epoch from n.ts - o.obs_at)) <= 1800)
    union all
    select extract(epoch from ts)::bigint, extract(epoch from ts)::bigint + 600, 'aviso'
      from rain_reports where device_id = ${deviceId} and raining and ts >= ${from} and ts < ${to}
    union all
    select extract(epoch from obs_at)::bigint - 3600, extract(epoch from obs_at)::bigint, 'modelo'
      from outdoor_obs o where device_id = ${deviceId} and source = 'open-meteo' and obs_at >= ${from} and obs_at < ${to} and precip >= 0.1
        and not exists (select 1 from outdoor_obs m where m.device_id = o.device_id and m.source = 'metar' and abs(extract(epoch from m.obs_at - o.obs_at)) <= 3600)
    order by 1`) as { a: string; b: string; src: string }[]
  const rain: { from: number; to: number; sources: string[] }[] = []
  for (const m of marks) {
    const a = Number(m.a), b = Number(m.b)
    const last = rain[rain.length - 1]
    if (last && a <= last.to) {
      last.to = Math.max(last.to, b)
      if (!last.sources.includes(m.src)) last.sources.push(m.src)
    } else rain.push({ from: a, to: b, sources: [m.src] })
  }

  const pc = (v: number | null) => (v == null ? null : Math.round(v * 100))
  return { from: range.from, to: range.to, h, points: rows.map((r) => ({ ts: Number(r.ts), m1: pc(r.m1), m2: pc(r.m2), m3: pc(r.m3) })), rain }
})
