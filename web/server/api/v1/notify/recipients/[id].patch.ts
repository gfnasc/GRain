// Altera nome, telefone, chave ou se está ativo. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const id = Number(getRouterParam(event, 'id'))
  const b = await readBody<{ name?: string; phone?: string; apikey?: string; enabled?: boolean }>(event)
  const sql = useSql()
  const [cur] = (await sql`select id, name, phone, apikey, enabled from recipients where id = ${id}`) as any[]
  if (!cur) throw createError({ statusCode: 404, message: 'destinatário não encontrado' })
  const name = b.name !== undefined ? b.name.trim().slice(0, 40) : cur.name
  const phone = b.phone !== undefined ? normalizePhone(b.phone) : cur.phone
  const apikey = b.apikey ? b.apikey.trim() : cur.apikey
  const enabled = typeof b.enabled === 'boolean' ? b.enabled : cur.enabled
  if (!name) throw createError({ statusCode: 422, message: 'informe o nome' })
  if (!phone) throw createError({ statusCode: 422, message: 'telefone no formato internacional, ex.: +5511999999999' })
  if (!/^[A-Za-z0-9]{4,32}$/.test(apikey)) throw createError({ statusCode: 422, message: 'chave do CallMeBot inválida' })
  const [r] = (await sql`update recipients set name = ${name}, phone = ${phone}, apikey = ${apikey}, enabled = ${enabled}
    where id = ${id} returning id, name, phone, enabled`) as any[]
  return { ...r, has_key: true }
})
