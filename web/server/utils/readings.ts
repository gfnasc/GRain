// Consultas comuns às leituras da estação.

export type LatestRow = { ts: Date; temp: number | null; hum: number | null; pres_slp: number | null }

export async function latestRow(deviceId: string): Promise<LatestRow | null> {
  const sql = useSql()
  const r = (await sql`select ts, temp, hum, pres_slp from readings where device_id = ${deviceId} order by ts desc limit 1`) as any[]
  return r[0] ? { ...r[0], ts: new Date(r[0].ts) } : null
}

// Variação da pressão nas últimas 3 h (média mais recente menos a de ~3 h antes).
export async function pressureDelta3h(deviceId: string, latest: LatestRow | null): Promise<number | null> {
  if (!latest?.pres_slp) return null
  const sql = useSql()
  const past = (await sql`
    select pres_slp from readings
    where device_id = ${deviceId} and pres_slp is not null
      and ts between ${new Date(latest.ts.getTime() - 3.5 * 3600_000).toISOString()} and ${new Date(latest.ts.getTime() - 2.5 * 3600_000).toISOString()}
    order by abs(extract(epoch from ts) - ${latest.ts.getTime() / 1000 - 3 * 3600}) limit 1`) as any[]
  return past[0] ? latest.pres_slp - past[0].pres_slp : null
}

export type Placement = { place: 'sacada' | 'escritorio'; from: number; to: number | null }

// Períodos de cada local que se sobrepõem ao intervalo (epoch s; to = null é o local atual).
export async function placementsIn(deviceId: string, from: Date, to: Date): Promise<Placement[]> {
  const sql = useSql()
  const rows = (await sql`
    select place, extract(epoch from started_at)::bigint as from_s, extract(epoch from ended_at)::bigint as to_s
    from placements
    where device_id = ${deviceId} and started_at < ${to.toISOString()} and (ended_at is null or ended_at > ${from.toISOString()})
    order by started_at`) as { place: Placement['place']; from_s: string; to_s: string | null }[]
  return rows.map((r) => ({ place: r.place, from: Number(r.from_s), to: r.to_s == null ? null : Number(r.to_s) }))
}

export async function currentPlace(deviceId: string): Promise<Placement['place'] | null> {
  const sql = useSql()
  const r = (await sql`select place from placements where device_id = ${deviceId} and ended_at is null`) as { place: Placement['place'] }[]
  return r[0]?.place ?? null
}
