// Estação e cópia da configuração enviada por ela, para a aba Estação. Exige login ou token de leitura.
// Somente leitura: a configuração é feita na página local do ESP32.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const sql = useSql()
  const rows = (await sql`
    select id, name, last_seen_at, last_status, station_config, station_config_at
    from devices order by created_at limit 1`) as Record<string, any>[]
  const d = rows[0]
  if (!d) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return {
    device: { id: d.id as string, name: d.name as string, last_seen_at: d.last_seen_at as string | null, status: d.last_status as Record<string, unknown> | null, place: await currentPlace(d.id) },
    config: (d.station_config as StationConfig | null) ?? null,
    config_at: d.station_config_at as string | null,
  }
})
