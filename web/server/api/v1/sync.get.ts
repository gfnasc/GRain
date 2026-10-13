// Horários das atualizações, para os contadores do painel. Exige login.
// Estimativas: a estação envia uma média de 5 min logo que o intervalo fecha; a previsão de
// chuva é recalculada a cada 10 min (no primeiro envio de cada intervalo de 10 min); o METAR
// é conferido a cada envio e o aeroporto publica de hora em hora (ou antes, se o tempo mudar);
// Open-Meteo e TAF a cada 30 min.

const INTERVAL_S = 300
const RAIN_S = 600
const MODEL_S = 30 * 60

export default defineEventHandler(async (event) => {
  await requireUserSession(event)
  const deviceId = await defaultDeviceId()
  if (!deviceId) throw createError({ statusCode: 404, message: 'nenhuma estação cadastrada' })
  const sql = useSql()
  const [d] = (await sql`
    select last_seen_at, outdoor,
      (select max(ts) from readings where device_id = ${deviceId}) as last_reading,
      (select max(issued_at) from rain_predictions where device_id = ${deviceId}) as last_rain,
      (select max(obs_at) from outdoor_obs where device_id = ${deviceId} and source = 'metar') as last_metar
    from devices where id = ${deviceId}`) as any[]
  const ms = (v: unknown) => (v ? new Date(v as string).getTime() : null)
  const now = Date.now()
  const lastReading = ms(d.last_reading)
  // Próximo envio: fim do intervalo seguinte ao da última média (ela é enviada quando o seu intervalo fecha).
  let nextReading = lastReading ? lastReading + 2 * INTERVAL_S * 1000 : null
  while (nextReading && nextReading < now - 60_000) nextReading += INTERVAL_S * 1000
  const lastRain = ms(d.last_rain)
  let nextRain = lastRain ? lastRain + RAIN_S * 1000 : null
  while (nextRain && nextRain < now - 60_000) nextRain += RAIN_S * 1000
  const lastMetar = ms(d.last_metar)
  const nextMetar = lastMetar ? Math.max(lastMetar + 3600_000, now) : null
  const modelAt = d.outdoor?.om_try ?? null
  const tafAt = d.outdoor?.taf?.at ?? null
  return {
    now,
    interval_s: INTERVAL_S,
    station: { last: ms(d.last_seen_at), next: nextReading },
    rain: { last: lastRain, next: nextRain },
    metar: { last: lastMetar, next: nextMetar },
    model: { last: modelAt, next: modelAt ? modelAt + MODEL_S * 1000 : null },
    taf: { last: tafAt, next: tafAt ? tafAt + MODEL_S * 1000 : null },
  }
})
