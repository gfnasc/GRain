// Configuração das notificações, destinatários (sem a chave) e últimos envios. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const sql = useSql()
  const { cfg } = await loadNotify()
  const recipients = (await sql`select id, name, phone, enabled, length(apikey) > 0 as has_key from recipients order by id`) as any[]
  const log = (await sql`select slot, kind, type, status, detail, message, created_at from notifications
    order by created_at desc limit 20`) as any[]
  return { config: cfg, recipients, log }
})
