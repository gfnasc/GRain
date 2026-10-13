// Condições de fora (Outdoor) para o painel: METAR do aeroporto mais próximo ou, se ele
// estiver atrasado, a condição atual do Open-Meteo. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return latestOutdoor(deviceId)
})
