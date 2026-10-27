// Placar dos métodos de chuva nos últimos 30 dias, o teste de 30 dias (situação, placar desde o
// começo e duelos nas mesmas rodadas) e o que o M2T e o M4 aprenderam. Exige login ou token de leitura.

export default defineEventHandler(async (event) => {
  await requireReadAccess(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const sql = useSql()
  const [pending] = (await sql`select count(*)::int as n from rain_predictions where device_id = ${deviceId} and outcome is null`) as { n: number }[]
  const trial = await rainTrial(deviceId)
  const [scores, trialScores, pairs, learned] = await Promise.all([
    rainScores(deviceId),
    trial.started_at ? rainScores(deviceId, 30, trial.started_at) : Promise.resolve([]),
    trial.started_at ? rainTrialPairs(deviceId, trial.started_at, trial.ends_at!) : Promise.resolve([]),
    learnedSummary(deviceId),
  ])
  return { days: 30, min_n: MIN_SCORE_N, min_events: MIN_SCORE_EVENTS, pending: pending?.n ?? 0, scores, trial: { ...trial, scores: trialScores, pairs }, learned }
})
