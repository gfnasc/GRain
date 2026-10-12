// Envia agora uma mensagem de teste (para todos os ativos ou um destinatário). Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const b = await readBody<{ type?: MessageType; recipient_id?: number }>(event)
  const type = b?.type ?? 'current'
  if (!MESSAGE_TYPES.includes(type)) throw createError({ statusCode: 400, message: 'tipo desconhecido' })
  const { deviceId, cfg } = await loadNotify()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const text = await composeMessage(type, deviceId, cfg, useRuntimeConfig(event).public.tz as string)
  const res = await deliver(`teste:${Date.now()}`, 'test', type, `🧪 Teste\n${text}`, b?.recipient_id)
  return { ...res, text }
})
