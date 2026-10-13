// Exporta as médias de 10 min em CSV. Exige login.
// Mesmos parâmetros de intervalo de /api/v1/readings.
//   ?excel=1  separador ";" e vírgula decimal (Excel em português)

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const range = await resolveRange(event)
  const tz = useRuntimeConfig(event).public.tz as string
  const excel = getQuery(event).excel === '1'
  const deviceId = await defaultDeviceId()

  const sql = useSql()
  const rows = deviceId
    ? ((await sql`
        select to_char(ts at time zone 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as utc,
               to_char(ts at time zone ${tz}, 'YYYY-MM-DD HH24:MI') as local,
               temp, hum, pres_slp, dew,
               coalesce((select p.place from placements p where p.device_id = readings.device_id and readings.ts >= p.started_at and (p.ended_at is null or readings.ts < p.ended_at)), '') as place
        from readings
        where device_id = ${deviceId} and ts >= ${range.from.toISOString()} and ts < ${range.to.toISOString()}
        order by ts`) as Record<string, any>[])
    : []

  const sep = excel ? ';' : ','
  const fmt = (v: unknown, d: number) => {
    if (v === null || v === undefined) return ''
    const s = Number(v).toFixed(d)
    return excel ? s.replace('.', ',') : s
  }
  const lines = [
    ['inicio_utc', `inicio_local (${tz})`, 'temperatura_c', 'umidade_pct', 'pressao_nivel_mar_hpa', 'ponto_orvalho_c', 'local'].join(sep),
    ...rows.map((r) => [r.utc, r.local, fmt(r.temp, 2), fmt(r.hum, 1), fmt(r.pres_slp, 2), fmt(r.dew, 1), r.place].join(sep)),
  ]

  const day = (d: Date) => d.toISOString().slice(0, 10).replaceAll('-', '')
  const name = `grain-${day(range.from)}-${day(range.to)}${excel ? '-excel' : ''}.csv`
  setHeader(event, 'Content-Type', 'text/csv; charset=utf-8')
  setHeader(event, 'Content-Disposition', `attachment; filename="${name}"`)
  setHeader(event, 'Cache-Control', 'no-store')
  // BOM para o Excel reconhecer UTF-8 (acentos do cabeçalho).
  return (excel ? '﻿' : '') + lines.join('\r\n') + '\r\n'
})
