// Previsão para o painel (aba Agora). Exige login ou token de leitura.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const { deviceId, site } = await loadSite()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return buildForecast(deviceId, site, useRuntimeConfig(event).public.tz as string)
})
