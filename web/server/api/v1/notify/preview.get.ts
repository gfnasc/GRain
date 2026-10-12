// Monta a mensagem de um tipo com os dados de agora, sem enviar. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const type = getQuery(event).type as MessageType
  if (!MESSAGE_TYPES.includes(type)) throw createError({ statusCode: 400, message: 'tipo desconhecido' })
  const { deviceId, cfg } = await loadNotify()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return { type, text: await composeMessage(type, deviceId, cfg, useRuntimeConfig(event).public.tz as string) }
})
