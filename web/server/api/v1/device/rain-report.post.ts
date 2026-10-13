// Botão "Está chovendo aqui" na página local da estação, repassado pelo ESP32.
// Autenticação: token da estação.

export default defineEventHandler(async (event) => {
  const deviceId = await requireDevice(event)
  return { ok: true, ts: await reportRain(deviceId, 'estacao') }
})
