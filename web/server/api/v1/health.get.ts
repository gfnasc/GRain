// Saúde da estação num período (padrão 72 h), para o teste de robustez. Exige login.
// Reinícios (pelo contador da estação ou pelo tempo ligado que volta a zero) e seus motivos,
// buracos no histórico (intervalos de mais de 15 min sem média), memória e sinal Wi-Fi.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const hours = Math.min(24 * 14, Math.max(1, Number(getQuery(event).hours ?? 72) || 72))
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const sql = useSql()
  const since = new Date(Date.now() - hours * 3600_000).toISOString()

  const log = (await sql`
    select ts, fw, uptime_s, heap, heap_min, rssi, queue, boots, reset from status_log
    where device_id = ${deviceId} and ts >= ${since} order by ts`) as any[]

  // Reinícios: o contador da estação subiu, ou o tempo ligado voltou para trás.
  const restarts: { ts: string; reason: string | null }[] = []
  for (let i = 1; i < log.length; i++) {
    const a = log[i - 1], b = log[i]
    const byBoots = a.boots != null && b.boots != null && b.boots > a.boots
    const byUptime = a.uptime_s != null && b.uptime_s != null && b.uptime_s < a.uptime_s
    if (byBoots || byUptime) restarts.push({ ts: new Date(b.ts).toISOString(), reason: b.reset })
  }

  // Buracos: intervalos de mais de 15 min entre médias recebidas.
  const gaps = (await sql`
    with r as (select ts, lead(ts) over (order by ts) as nxt from readings where device_id = ${deviceId} and ts >= ${since})
    select ts as from_ts, nxt as to_ts, extract(epoch from nxt - ts)::int as secs from r
    where nxt is not null and nxt - ts > interval '15 minutes' order by ts`) as { from_ts: string; to_ts: string; secs: number }[]
  const [cov] = (await sql`
    select count(*)::int as n, min(ts) as first from readings where device_id = ${deviceId} and ts >= ${since}`) as { n: number; first: string | null }[]

  const heaps = log.map((l) => l.heap_min).filter((v) => v != null) as number[]
  const rssi = log.map((l) => l.rssi).filter((v) => v != null) as number[]
  const last = log[log.length - 1] ?? null
  const first = log[0] ?? null
  const spanH = first ? (Date.now() - new Date(first.ts).getTime()) / 3600_000 : 0
  const expected = Math.floor((Date.now() - new Date(cov?.first ?? Date.now()).getTime()) / 300_000)
  return {
    hours,
    observed_h: Math.round(spanH * 10) / 10,
    samples: log.length,
    fw: last?.fw ?? null,
    uptime_s: last?.uptime_s ?? null,
    last_reset: last?.reset ?? null,
    boots: last?.boots ?? null,
    restarts,
    gaps: gaps.map((g) => ({ from: new Date(g.from_ts).toISOString(), to: new Date(g.to_ts).toISOString(), minutes: Math.round(g.secs / 60) })),
    coverage: expected > 0 ? Math.min(100, Math.round(((cov?.n ?? 0) / expected) * 100)) : null,
    heap_min: heaps.length ? { first: heaps[0], last: heaps[heaps.length - 1], lowest: Math.min(...heaps) } : null,
    heap_now: last?.heap ?? null,
    rssi: rssi.length ? { avg: Math.round(rssi.reduce((a, b) => a + b, 0) / rssi.length), min: Math.min(...rssi) } : null,
  }
})
