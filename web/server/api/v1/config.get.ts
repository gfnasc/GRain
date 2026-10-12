// Configuração do display e da calibração para o editor do painel. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const sql = useSql()
  const rows = (await sql`
    select id, config, config_version, config_updated_at, config_applied_version, last_poll_at
    from devices order by created_at limit 1`) as Record<string, any>[]
  const d = rows[0]
  if (!d) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })

  return {
    device_id: d.id as string,
    version: d.config_version as number,
    config: (d.config as StationConfig | null) ?? DEFAULT_CONFIG,
    is_default: !d.config,
    updated_at: d.config_updated_at as string | null,
    applied_version: d.config_applied_version as number | null,
    last_poll_at: d.last_poll_at as string | null,
  }
})
