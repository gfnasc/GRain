// Leitura mais recente e estado da estação, para o painel. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const sql = useSql()

  const devices = (await sql`
    select id, name, last_seen_at, last_status, station_config->'calibration' as calibration from devices order by created_at limit 1`) as {
    calibration: { pres_auto?: boolean; pres_auto_offset?: number } | null
    id: string
    name: string
    last_seen_at: string | null
    last_status: Record<string, unknown> | null
  }[]
  const device = devices[0]
  if (!device) return { device: null, latest: null, recent: [] }

  const recent = (await sql`
    select extract(epoch from ts)::bigint as ts, temp, hum, pres_slp, dew
    from readings where device_id = ${device.id}
    order by ts desc limit 12`) as { ts: string; temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null }[]

  // Mínimas e máximas de 24 h sem o período na sacada (temperatura e umidade de lá não valem).
  const day = (await sql`
    select min(temp) as temp_min, max(temp) as temp_max, min(hum) as hum_min, max(hum) as hum_max, count(*)::int as n
    from readings where device_id = ${device.id} and ts > now() - interval '24 hours' and not exists (select 1 from placements p where p.device_id = readings.device_id and p.place = 'sacada' and readings.ts >= p.started_at and (p.ended_at is null or readings.ts < p.ended_at))`) as Record<string, number | null>[]

  const rows = recent.map((r) => ({ ...r, ts: Number(r.ts) }))
  return {
    device: { id: device.id, name: device.name, last_seen_at: device.last_seen_at, status: device.last_status, place: await currentPlace(device.id),
      pres_cal: device.calibration ? { auto: device.calibration.pres_auto !== false, offset: device.calibration.pres_auto_offset ?? 0 } : null },
    latest: rows[0] ?? null,
    recent: rows,
    last24h: day[0] ?? null,
  }
})
