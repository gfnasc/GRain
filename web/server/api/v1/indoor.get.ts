// Indoor (o ambiente da estação): conforto, ar seco, mofo, janela e atraso térmico. Exige login ou token de leitura.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return indoorInsights(deviceId)
})
