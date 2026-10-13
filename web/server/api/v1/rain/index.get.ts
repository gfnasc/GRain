// Chuva nas próximas horas: os três métodos, o destaque de cada janela e os avisos
// "Está chovendo aqui" das últimas 24 h. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const { deviceId, site } = await loadSite()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const sql = useSql()
  const reports = (await sql`
    select ts, via from rain_reports where device_id = ${deviceId} and ts > now() - interval '24 hours' order by ts desc`) as { ts: string; via: string }[]
  return {
    located: site.lat != null,
    now: await latestRain(deviceId),
    reports: reports.map((r) => ({ ts: new Date(r.ts).toISOString(), via: r.via })),
  }
})
