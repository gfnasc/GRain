// Saúde da estação para o teste de robustez (72 h), contada a partir da gravação do firmware
// atual (primeiro registro com a versão em uso). Ver utils/trials.ts. Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const hours = Math.min(24 * 14, Math.max(1, Number(getQuery(event).hours ?? HEALTH_TRIAL_H) || HEALTH_TRIAL_H))
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  return healthReport(deviceId, hours)
})
