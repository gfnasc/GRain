// Botão "Está chovendo aqui" no painel. Um aviso por intervalo de 10 min. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return { ok: true, ts: await reportRain(deviceId, 'painel') }
})
