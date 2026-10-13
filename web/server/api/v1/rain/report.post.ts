// Botões do painel: "Está chovendo aqui" (um aviso por intervalo de 10 min) e "Não choveu hoje"
// ({ kind: 'nao_choveu_hoje' }, ou { kind: 'nao_choveu_hoje', undo: true } para desfazer). Exige login.

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const body = await readBody<{ kind?: string; undo?: boolean } | null>(event).catch(() => null)
  if (body?.kind === 'nao_choveu_hoje') return { ok: true, ...(await reportNoRainToday(deviceId, 'painel', !!body.undo)) }
  return { ok: true, ts: await reportRain(deviceId, 'painel') }
})
