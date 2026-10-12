// Remove um destinatário. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const id = Number(getRouterParam(event, 'id'))
  const sql = useSql()
  const r = (await sql`delete from recipients where id = ${id} returning id`) as any[]
  if (!r.length) throw createError({ statusCode: 404, message: 'destinatário não encontrado' })
  return { ok: true }
})
