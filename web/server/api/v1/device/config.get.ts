// Consultado pelo ESP32 a cada minuto. Autenticação: token da estação.
//   ?v=<versão aplicada na estação>
// Responde 204 se a estação já tem a versão atual; senão {version, config}.
// Cada consulta registra a versão aplicada e a hora do contato.

export default defineEventHandler(async (event) => {
  const deviceId = await requireDevice(event)
  const v = Number(getQuery(event).v ?? 0)
  const applied = Number.isInteger(v) && v >= 0 ? v : null
  const sql = useSql()

  const rows = (await sql`
    update devices
       set last_poll_at = now(), config_applied_version = ${applied}
     where id = ${deviceId}
    returning config, config_version`) as { config: StationConfig | null; config_version: number }[]
  const row = rows[0]!

  if (!row.config || row.config_version === applied) {
    setResponseStatus(event, 204)
    return null
  }
  return { version: row.config_version, config: row.config }
})
