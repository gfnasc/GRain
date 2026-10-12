// Recebe um lote de médias de 10 min do ESP32.
// Autenticação: Authorization: Bearer <token do aparelho>.
// Horários repetidos são ignorados (o ESP32 pode reenviar após falha).

type Reading = { ts: number; temp?: number | null; hum?: number | null; pres_slp?: number | null; dew?: number | null }

const MAX_BATCH = 500
const MIN_TS = 1704067200 // 2024-01-01

function num(v: unknown, lo: number, hi: number): number | null {
  if (v === null || v === undefined) return null
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi) throw new Error('fora da faixa')
  return v
}

export default defineEventHandler(async (event) => {
  const deviceId = await requireDevice(event)
  const body = await readBody<{ readings?: Reading[]; status?: Record<string, unknown> }>(event)

  const list = body?.readings
  if (!Array.isArray(list)) throw createError({ statusCode: 400, message: 'readings deve ser uma lista' })
  if (list.length > MAX_BATCH) throw createError({ statusCode: 413, message: `máximo de ${MAX_BATCH} leituras por lote` })

  const maxTs = Math.floor(Date.now() / 1000) + 300
  const ts: string[] = [], temp: (number | null)[] = [], hum: (number | null)[] = []
  const pres: (number | null)[] = [], dew: (number | null)[] = []

  for (const [i, r] of list.entries()) {
    try {
      if (!Number.isInteger(r.ts) || r.ts < MIN_TS || r.ts > maxTs) throw new Error('ts inválido')
      ts.push(new Date(r.ts * 1000).toISOString())
      temp.push(num(r.temp, -40, 85))
      hum.push(num(r.hum, 0, 100))
      pres.push(num(r.pres_slp, 800, 1100))
      dew.push(num(r.dew, -60, 60))
    } catch (e) {
      throw createError({ statusCode: 422, message: `leitura ${i}: ${(e as Error).message}` })
    }
  }

  const sql = useSql()
  let accepted = 0
  if (ts.length) {
    const rows = await sql`
      insert into readings (device_id, ts, temp, hum, pres_slp, dew)
      select ${deviceId}, * from unnest(
        ${ts}::timestamptz[], ${temp}::real[], ${hum}::real[], ${pres}::real[], ${dew}::real[]
      )
      on conflict (device_id, ts) do nothing
      returning ts`
    accepted = rows.length
  }

  const status = body.status && typeof body.status === 'object' ? JSON.stringify(body.status).slice(0, 2000) : null
  await sql`update devices set last_seen_at = now(), last_status = coalesce(${status}::jsonb, last_status) where id = ${deviceId}`

  // Alertas avaliados com a média mais recente (falha aqui não afeta o recebimento).
  if (accepted) {
    try {
      const { cfg } = await loadNotify()
      await runAlerts(deviceId, cfg, useRuntimeConfig(event).public.tz as string)
    } catch (e) {
      console.error('alertas falharam:', e)
    }
  }

  return { accepted, duplicates: ts.length - accepted }
})
