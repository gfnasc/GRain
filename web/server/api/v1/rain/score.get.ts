// Placar dos métodos de chuva nos últimos 30 dias. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const sql = useSql()
  const [pending] = (await sql`select count(*)::int as n from rain_predictions where device_id = ${deviceId} and outcome is null`) as { n: number }[]
  return { days: 30, min_n: MIN_SCORE_N, min_events: MIN_SCORE_EVENTS, pending: pending?.n ?? 0, scores: await rainScores(deviceId) }
})
