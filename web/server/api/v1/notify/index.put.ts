// Salva agenda, alertas e local da previsão. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  let cfg: NotifyConfig
  try {
    cfg = validateNotifyConfig(await readBody(event))
  } catch (e) {
    throw createError({ statusCode: 422, message: (e as Error).message })
  }
  const sql = useSql()
  await sql`update devices set notify = ${JSON.stringify(cfg)}::jsonb
    where id = (select id from devices order by created_at limit 1)`
  return { config: cfg }
})
