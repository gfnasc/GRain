// Salva a configuração do display e da calibração. Exige login.
// Cada gravação gera uma nova versão; a estação aplica na próxima consulta (até 1 min).

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  let config: StationConfig
  try {
    config = validateStationConfig(await readBody(event))
  } catch (e) {
    throw createError({ statusCode: 422, message: (e as Error).message })
  }

  const sql = useSql()
  const rows = (await sql`
    update devices
       set config = ${JSON.stringify(config)}::jsonb,
           config_version = config_version + 1,
           config_updated_at = now()
     where id = (select id from devices order by created_at limit 1)
    returning config_version`) as { config_version: number }[]
  if (!rows[0]) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })

  return { version: rows[0].config_version, config }
})
