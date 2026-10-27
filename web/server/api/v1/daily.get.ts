// Resumo por dia (no fuso configurado): mínimas, máximas e médias. Exige login ou token de leitura.
// Aceita os mesmos parâmetros de intervalo de /api/v1/readings.
// Temperatura e umidade da sacada ficam de fora (ver placements); a pressão entra sempre.
// Traz também a mínima e a máxima de fora (Outdoor) de cada dia.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const range = await resolveRange(event)
  const tz = useRuntimeConfig(event).public.tz as string
  const deviceId = await defaultDeviceId()
  if (!deviceId) return { days: [] }

  const sql = useSql()
  const rows = (await sql`
    select to_char((ts at time zone ${tz})::date, 'YYYY-MM-DD') as day,
           min(temp) filter (where not sacada) as temp_min, max(temp) filter (where not sacada) as temp_max, avg(temp) filter (where not sacada) as temp_avg,
           min(hum) filter (where not sacada) as hum_min, max(hum) filter (where not sacada) as hum_max, avg(hum) filter (where not sacada) as hum_avg,
           min(pres_slp) as pres_min, max(pres_slp) as pres_max, avg(pres_slp) as pres_avg,
           count(*)::int as n, count(*) filter (where sacada)::int as n_sacada,
           count(distinct date_bin('10 minutes', ts, timestamp '2000-01-01'))::int as buckets
    from (select *, exists (select 1 from placements p where p.device_id = readings.device_id and p.place = 'sacada' and readings.ts >= p.started_at and (p.ended_at is null or readings.ts < p.ended_at)) as sacada from readings) readings
    where device_id = ${deviceId} and ts >= ${range.from.toISOString()} and ts < ${range.to.toISOString()}
    group by 1 order by 1 desc`) as Record<string, any>[]

  // Temperatura de fora por dia: METAR; nos dias sem METAR, Open-Meteo.
  const outRows = (await sql`
    select to_char((obs_at at time zone ${tz})::date, 'YYYY-MM-DD') as day,
      coalesce(min(temp) filter (where source = 'metar'), min(temp) filter (where source = 'open-meteo')) as out_min,
      coalesce(max(temp) filter (where source = 'metar'), max(temp) filter (where source = 'open-meteo')) as out_max
    from outdoor_obs
    where device_id = ${deviceId} and temp is not null and obs_at >= ${range.from.toISOString()} and obs_at < ${range.to.toISOString()}
    group by 1`) as { day: string; out_min: number | null; out_max: number | null }[]
  const outByDay = new Map(outRows.map((r) => [r.day, r]))
  const num = (v: unknown) => (v === null || v === undefined ? null : Math.round(Number(v) * 1000) / 1000)
  return {
    days: rows.map((r) => ({
      day: r.day as string,
      temp_min: num(r.temp_min), temp_max: num(r.temp_max), temp_avg: num(r.temp_avg),
      hum_min: num(r.hum_min), hum_max: num(r.hum_max), hum_avg: num(r.hum_avg),
      pres_min: num(r.pres_min), pres_max: num(r.pres_max), pres_avg: num(r.pres_avg),
      n: Number(r.n),
      buckets: Number(r.buckets),
      n_sacada: Number(r.n_sacada),
      out_min: num(outByDay.get(r.day)?.out_min),
      out_max: num(outByDay.get(r.day)?.out_max),
    })),
  }
})
