export type Site = { lat: number | null; lon: number | null; label: string }

// Localização da estação, usada para os dados externos.
export async function loadSite(): Promise<{ deviceId: string | null; site: Site }> {
  const sql = useSql()
  const rows = (await sql`select id, site from devices order by created_at limit 1`) as { id: string; site: Partial<Site> | null }[]
  const s = rows[0]?.site ?? {}
  return { deviceId: rows[0]?.id ?? null, site: { lat: s.lat ?? null, lon: s.lon ?? null, label: s.label ?? '' } }
}

// O painel mostra uma estação: a primeira cadastrada.
export async function defaultDeviceId(): Promise<string | null> {
  const sql = useSql()
  const rows = (await sql`select id from devices order by created_at limit 1`) as { id: string }[]
  return rows[0]?.id ?? null
}
