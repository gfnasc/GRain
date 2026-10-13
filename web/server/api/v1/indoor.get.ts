// Indoor (o ambiente da estação): conforto, ar seco, mofo, janela e atraso térmico. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return indoorInsights(deviceId)
})
