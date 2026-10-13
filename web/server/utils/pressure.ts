// Desvio entre a pressão da estação e o QNH do aeroporto (METAR).
// Usa a pressão sem calibração (pres_raw; nas médias antigas, sem ela, pres_slp, que não
// tinha ajuste) para não realimentar o próprio ajuste. Cada METAR é comparado com as médias
// dos 10 min anteriores a ele (duas de 5 min, ou uma de 10 nas médias antigas); o resultado é
// a mediana dos últimos 7 dias.

const MIN_PAIRS = 12

export async function pressureOffset(deviceId: string): Promise<{ value: number; n: number; sd: number } | null> {
  const sql = useSql()
  const [r] = (await sql`
    with pairs as (
      select o.obs_at, o.pres - avg(coalesce(r.pres_raw, r.pres_slp)) as d
      from outdoor_obs o
      join readings r on r.device_id = o.device_id
        and r.ts >= o.obs_at - interval '10 minutes' and r.ts < o.obs_at
      where o.device_id = ${deviceId} and o.source = 'metar' and o.pres is not null
        and o.obs_at > now() - interval '7 days' and coalesce(r.pres_raw, r.pres_slp) is not null
      group by o.obs_at, o.pres)
    select count(*)::int as n, percentile_cont(0.5) within group (order by d) as median, coalesce(stddev(d), 0) as sd
    from pairs`) as { n: number; median: number | null; sd: number }[]
  if (!r || r.n < MIN_PAIRS || r.median == null) return null
  return { value: Math.round(r.median * 100) / 100, n: r.n, sd: Math.round(r.sd * 100) / 100 }
}
