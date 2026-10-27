// Condições de fora (Outdoor) para o painel: METAR do aeroporto mais próximo ou, se ele
// estiver atrasado, a condição atual do Open-Meteo. Exige login ou token de leitura.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return latestOutdoor(deviceId)
})
