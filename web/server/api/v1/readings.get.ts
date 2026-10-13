// Série histórica para os gráficos. Exige login.
// Agrega por intervalo (10 min, 1 h, 3 h ou 1 dia, conforme o tamanho do
// período) com média, mínima e máxima de cada grandeza, e devolve os
// extremos exatos do período com o horário em que ocorreram.
// Temperatura e umidade medidas na sacada aparecem nos gráficos (marcadas) mas
// ficam fora dos extremos e das médias; a pressão vale em qualquer local.
// Cada ponto traz também a temperatura de fora (out_temp): METAR do aeroporto, com o
// Open-Meteo nos buracos, interpolada entre observações próximas.

type Extreme = { v: number; ts: number } | null

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const range = await resolveRange(event)
  const step = stepFor(range)
  const tz = useRuntimeConfig(event).public.tz as string
  const deviceId = await defaultDeviceId()
  if (!deviceId) return { from: range.from, to: range.to, step_s: step, points: [], placements: [], summary: null }

  const sql = useSql()
  const from = range.from.toISOString()
  const to = range.to.toISOString()
  const stepInterval = `${step} seconds`

  const points = (await sql`
    select extract(epoch from date_bin(${stepInterval}::interval, ts, timestamp '2000-01-01' at time zone ${tz}))::bigint as ts,
           avg(temp) as temp, min(temp) as temp_min, max(temp) as temp_max,
           avg(hum) as hum, min(hum) as hum_min, max(hum) as hum_max,
           avg(pres_slp) as pres, min(pres_slp) as pres_min, max(pres_slp) as pres_max,
           avg(dew) as dew,
           count(*)::int as n
    from readings
    where device_id = ${deviceId} and ts >= ${from} and ts < ${to}
    group by 1 order by 1`) as Record<string, number | string | null>[]

  // Extremos exatos (não as médias dos baldes).
  const [ext] = (await sql`
    with r as (
      select ts, temp, hum, pres_slp, exists (select 1 from placements p where p.device_id = readings.device_id and p.place = 'sacada' and readings.ts >= p.started_at and (p.ended_at is null or readings.ts < p.ended_at)) as sacada
      from readings where device_id = ${deviceId} and ts >= ${from} and ts < ${to})
    select
      (select json_build_object('v', temp, 'ts', extract(epoch from ts)::bigint) from r where temp is not null and not sacada order by temp asc, ts limit 1) as temp_min,
      (select json_build_object('v', temp, 'ts', extract(epoch from ts)::bigint) from r where temp is not null and not sacada order by temp desc, ts limit 1) as temp_max,
      (select json_build_object('v', hum, 'ts', extract(epoch from ts)::bigint) from r where hum is not null and not sacada order by hum asc, ts limit 1) as hum_min,
      (select json_build_object('v', hum, 'ts', extract(epoch from ts)::bigint) from r where hum is not null and not sacada order by hum desc, ts limit 1) as hum_max,
      (select json_build_object('v', pres_slp, 'ts', extract(epoch from ts)::bigint) from r where pres_slp is not null order by pres_slp asc, ts limit 1) as pres_min,
      (select json_build_object('v', pres_slp, 'ts', extract(epoch from ts)::bigint) from r where pres_slp is not null order by pres_slp desc, ts limit 1) as pres_max,
      (select avg(temp) from r where not sacada) as temp_avg,
      (select avg(hum) from r where not sacada) as hum_avg,
      (select count(*)::int from r where sacada) as n_sacada,
      (select avg(pres_slp) from r) as pres_avg,
      (select count(*)::int from r) as n`) as Record<string, any>[]

  const placements = await placementsIn(deviceId, range.from, range.to)

  const out = (await sql`
    select extract(epoch from obs_at)::bigint as ts, temp, source from outdoor_obs
    where device_id = ${deviceId} and temp is not null
      and obs_at >= ${new Date(range.from.getTime() - 2 * 3600_000).toISOString()} and obs_at < ${new Date(range.to.getTime() + 2 * 3600_000).toISOString()}
    order by obs_at`) as { ts: string; temp: number; source: string }[]
  // METAR onde houver; nos buracos (aeroporto fechado à noite, atraso) entra o Open-Meteo.
  const metarOut = out.filter((o) => o.source === 'metar').map((o) => ({ ts: Number(o.ts), v: o.temp }))
  const modelOut = out.filter((o) => o.source === 'open-meteo').map((o) => ({ ts: Number(o.ts), v: o.temp }))
  const outSeries = [...metarOut, ...modelOut.filter((m) => !metarOut.some((o) => Math.abs(o.ts - m.ts) < 75 * 60))].sort((x, y) => x.ts - y.ts)
  const outdoorAt = (start: number) => outdoorValue(outSeries, start, step)
  const num = (v: unknown) => (v === null || v === undefined ? null : Math.round(Number(v) * 1000) / 1000)
  const extreme = (e: any): Extreme => (e ? { v: Number(e.v), ts: Number(e.ts) } : null)

  return {
    from: range.from,
    to: range.to,
    step_s: step,
    points: points.map((p) => ({
      ts: Number(p.ts),
      temp: num(p.temp), temp_min: num(p.temp_min), temp_max: num(p.temp_max),
      hum: num(p.hum), hum_min: num(p.hum_min), hum_max: num(p.hum_max),
      pres: num(p.pres), pres_min: num(p.pres_min), pres_max: num(p.pres_max),
      dew: num(p.dew),
      out_temp: outdoorAt(Number(p.ts)),
      n: Number(p.n),
    })),
    placements,
    summary: {
      n: ext?.n ?? 0,
      n_sacada: ext?.n_sacada ?? 0,
      expected: Math.floor((range.to.getTime() - range.from.getTime()) / 600_000),
      temp: { min: extreme(ext?.temp_min), max: extreme(ext?.temp_max), avg: num(ext?.temp_avg) },
      hum: { min: extreme(ext?.hum_min), max: extreme(ext?.hum_max), avg: num(ext?.hum_avg) },
      pres: { min: extreme(ext?.pres_min), max: extreme(ext?.pres_max), avg: num(ext?.pres_avg) },
    },
  }
})

// Temperatura de fora para o intervalo [start, start + step): média das observações dentro
// dele; sem nenhuma, interpolação linear no meio do intervalo entre vizinhas a até 2 h.
function outdoorValue(series: { ts: number; v: number }[], start: number, step: number): number | null {
  if (!series.length) return null
  const inside = series.filter((o) => o.ts >= start && o.ts < start + step)
  if (inside.length) return Math.round((inside.reduce((a, o) => a + o.v, 0) / inside.length) * 10) / 10
  const t = start + step / 2
  let a: { ts: number; v: number } | undefined
  let b: { ts: number; v: number } | undefined
  for (const o of series) {
    if (o.ts <= t) a = o
    else { b = o; break }
  }
  if (a && b && b.ts - a.ts <= 2 * 3600) return Math.round((a.v + ((b.v - a.v) * (t - a.ts)) / (b.ts - a.ts)) * 10) / 10
  const near = [a, b].filter((o) => o && Math.abs(o.ts - t) <= 45 * 60) as { ts: number; v: number }[]
  return near.length ? near[0]!.v : null
}
