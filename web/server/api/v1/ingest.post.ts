import { waitUntil } from '@vercel/functions'

// Recebe um lote de médias de 10 min do ESP32, o estado da estação e a cópia da
// configuração (display, calibração e localização), que é feita só no ESP32.
// Autenticação: Authorization: Bearer <token do aparelho>.
// Horários repetidos são ignorados (o ESP32 pode reenviar após falha). O lote pode vir
// vazio quando a estação só quer mandar a cópia da configuração.

type Reading = { ts: number; temp?: number | null; hum?: number | null; pres_slp?: number | null; pres_raw?: number | null; dew?: number | null }

const MAX_BATCH = 500
const MIN_TS = 1704067200 // 2024-01-01

function num(v: unknown, lo: number, hi: number): number | null {
  if (v === null || v === undefined) return null
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi) throw new Error('fora da faixa')
  return v
}

export default defineEventHandler(async (event) => {
  const deviceId = await requireDevice(event)
  const body = await readBody<{ readings?: Reading[]; status?: Record<string, unknown>; config?: unknown }>(event)

  const list = body?.readings
  if (!Array.isArray(list)) throw createError({ statusCode: 400, message: 'readings deve ser uma lista' })
  if (list.length > MAX_BATCH) throw createError({ statusCode: 413, message: `máximo de ${MAX_BATCH} leituras por lote` })

  const maxTs = Math.floor(Date.now() / 1000) + 300
  const ts: string[] = [], temp: (number | null)[] = [], hum: (number | null)[] = []
  const pres: (number | null)[] = [], presRaw: (number | null)[] = [], dew: (number | null)[] = []

  for (const [i, r] of list.entries()) {
    try {
      if (!Number.isInteger(r.ts) || r.ts < MIN_TS || r.ts > maxTs) throw new Error('ts inválido')
      ts.push(new Date(r.ts * 1000).toISOString())
      temp.push(num(r.temp, -40, 85))
      hum.push(num(r.hum, 0, 100))
      pres.push(num(r.pres_slp, 800, 1100))
      presRaw.push(num(r.pres_raw, 800, 1100))
      dew.push(num(r.dew, -60, 60))
    } catch (e) {
      throw createError({ statusCode: 422, message: `leitura ${i}: ${(e as Error).message}` })
    }
  }

  const sql = useSql()
  let accepted = 0
  if (ts.length) {
    const rows = await sql`
      insert into readings (device_id, ts, temp, hum, pres_slp, dew, pres_raw)
      select ${deviceId}, * from unnest(
        ${ts}::timestamptz[], ${temp}::real[], ${hum}::real[], ${pres}::real[], ${dew}::real[], ${presRaw}::real[]
      )
      on conflict (device_id, ts) do nothing
      returning ts`
    accepted = rows.length
  }

  const statusJson = body.status && typeof body.status === 'object' ? JSON.stringify(body.status) : null
  const status = statusJson && statusJson.length <= 2000 ? statusJson : null
  await sql`update devices set last_seen_at = now(), last_status = coalesce(${status}::jsonb, last_status) where id = ${deviceId}`

  // Cópia da configuração: inválida é ignorada (as médias já foram gravadas).
  if (body.config !== undefined) {
    try {
      const cfg = validateStationConfig(body.config)
      const loc = cfg.location
      const site = loc.lat != null && loc.lon != null ? JSON.stringify(loc) : null
      await sql`
        update devices set station_config = ${JSON.stringify(cfg)}::jsonb, station_config_at = now(), site = ${site}::jsonb
        where id = ${deviceId}`
    } catch (e) {
      console.warn(`configuração de ${deviceId} ignorada:`, (e as Error).message)
    }
  }

  // Dados de fora (Outdoor): cada envio da estação serve de relógio para a coleta, que roda
  // depois da resposta (waitUntil) para a estação não esperar pelas APIs externas.
  // A previsão de chuva desta rodada é calculada antes da resposta (é rápida: usa os dados de
  // fora já coletados), para a estação receber a mesma previsão que o painel mostra. A coleta
  // de fora e a verificação das janelas que terminaram rodam depois, para a próxima rodada.
  await computeRain(deviceId).catch((e) => console.warn('previsão de chuva falhou:', (e as Error).message))
  const job = (async () => {
    await collectOutdoor(deviceId).catch((e) => console.warn('coleta Outdoor falhou:', (e as Error).message))
    await verifyRain(deviceId).catch((e) => console.warn('verificação da chuva falhou:', (e as Error).message))
  })()
  if (process.env.VERCEL) waitUntil(job)
  else await job

  // Desvio da pressão em relação ao aeroporto: a estação aplica se a calibração automática
  // estiver ligada (só vem quando há pares suficientes).
  let presOffset: number | undefined
  try {
    presOffset = (await pressureOffset(deviceId))?.value
  } catch (e) {
    console.warn('desvio da pressão falhou:', (e as Error).message)
  }

  // Previsão de chuva desta rodada para a página e o display da estação.
  let rain: ReturnType<typeof rainForDevice>
  try {
    rain = rainForDevice(await latestRain(deviceId))
  } catch (e) {
    console.warn('previsão de chuva para a estação falhou:', (e as Error).message)
  }

  return { accepted, duplicates: ts.length - accepted, ...(presOffset != null ? { pres_offset: presOffset } : {}), ...(rain ? { rain } : {}) }
})
