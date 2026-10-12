// Resumo por dia (no fuso configurado): mínimas, máximas e médias. Exige login.
// Aceita os mesmos parâmetros de intervalo de /api/v1/readings.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const range = await resolveRange(event)
  const tz = useRuntimeConfig(event).public.tz as string
  const deviceId = await defaultDeviceId()
  if (!deviceId) return { days: [] }

  const sql = useSql()
  const rows = (await sql`
    select to_char((ts at time zone ${tz})::date, 'YYYY-MM-DD') as day,
           min(temp) as temp_min, max(temp) as temp_max, avg(temp) as temp_avg,
           min(hum) as hum_min, max(hum) as hum_max, avg(hum) as hum_avg,
           min(pres_slp) as pres_min, max(pres_slp) as pres_max, avg(pres_slp) as pres_avg,
           count(*)::int as n
    from readings
    where device_id = ${deviceId} and ts >= ${range.from.toISOString()} and ts < ${range.to.toISOString()}
    group by 1 order by 1 desc`) as Record<string, any>[]

  const num = (v: unknown) => (v === null || v === undefined ? null : Math.round(Number(v) * 1000) / 1000)
  return {
    days: rows.map((r) => ({
      day: r.day as string,
      temp_min: num(r.temp_min), temp_max: num(r.temp_max), temp_avg: num(r.temp_avg),
      hum_min: num(r.hum_min), hum_max: num(r.hum_max), hum_avg: num(r.hum_avg),
      pres_min: num(r.pres_min), pres_max: num(r.pres_max), pres_avg: num(r.pres_avg),
      n: Number(r.n),
    })),
  }
})
