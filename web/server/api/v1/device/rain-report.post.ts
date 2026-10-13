// Botões da página local da estação, repassados pelo ESP32: "Está chovendo aqui" e
// "Não choveu hoje" ({ kind: 'nao_choveu_hoje' }). Autenticação: token da estação.

export default defineEventHandler(async (event) => {
  const deviceId = await requireDevice(event)
  const body = await readBody<{ kind?: string } | null>(event).catch(() => null)
  if (body?.kind === 'nao_choveu_hoje') return { ok: true, ...(await reportNoRainToday(deviceId, 'estacao')) }
  return { ok: true, ts: await reportRain(deviceId, 'estacao') }
})
