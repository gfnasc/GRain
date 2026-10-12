// Cadastra um destinatário. Exige login. A chave do CallMeBot nunca volta ao navegador.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const b = await readBody<{ name?: string; phone?: string; apikey?: string }>(event)
  const name = (b?.name ?? '').trim().slice(0, 40)
  const phone = normalizePhone(b?.phone ?? '')
  const apikey = (b?.apikey ?? '').trim()
  if (!name) throw createError({ statusCode: 422, message: 'informe o nome' })
  if (!phone) throw createError({ statusCode: 422, message: 'telefone no formato internacional, ex.: +5511999999999' })
  if (!/^[A-Za-z0-9]{4,32}$/.test(apikey)) throw createError({ statusCode: 422, message: 'chave do CallMeBot inválida' })
  const sql = useSql()
  const [r] = (await sql`insert into recipients (name, phone, apikey) values (${name}, ${phone}, ${apikey})
    returning id, name, phone, enabled`) as any[]
  return { ...r, has_key: true }
})
