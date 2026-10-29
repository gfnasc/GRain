// Filtro de leituras fisicamente impossíveis, aplicado em cada envio antes de gravar. Protege o
// histórico, a tendência da pressão (previsão de chuva), o treino do M2T/M4 e a calibração pelo
// QNH contra leituras corrompidas — ex.: 29/10, outro aparelho no barramento I2C fez o BMP280
// marcar 1043 hPa (+24, o equivalente a subir 200 m em 5 min).
// Regras (folgadas de propósito: só o impossível sai; saltos reais, como a frente de rajada de
// uma tempestade, de 1–5 hPa, passam):
//   Pressão: mais de 10 hPa do QNH do aeroporto (boletim das últimas 2 h); sem boletim, salto de
//            mais de 6 hPa em relação à última pressão boa dos 15 min anteriores.
//   AHT20:   salto de mais de 8 °C ou de 30 pontos de umidade em relação ao último valor bom dos
//            15 min anteriores (descarta temperatura, umidade e orvalho juntos).
// Nada é inventado nem perdido: o valor sai da média (null = sem leitura) e vai para a tabela
// rejected_readings com o motivo, podendo ser restaurado.

const PRES_VS_QNH = 10
const PRES_JUMP = 6
const TEMP_JUMP = 8
const HUM_JUMP = 30
const WINDOW_S = 900

type Num = (number | null)[]

export async function sanitizeReadings(deviceId: string, ts: string[], temp: Num, hum: Num, pres: Num, presRaw: Num, dew: Num): Promise<number> {
  const sql = useSql()
  const first = ts.reduce((a, b) => (a < b ? a : b))
  const [prev] = (await sql`
    select
      (select row(extract(epoch from ts), pres_slp)::text from readings where device_id = ${deviceId} and ts < ${first} and pres_slp is not null order by ts desc limit 1) as p,
      (select row(extract(epoch from ts), temp, hum)::text from readings where device_id = ${deviceId} and ts < ${first} and temp is not null order by ts desc limit 1) as t,
      (select row(extract(epoch from obs_at), pres)::text from outdoor_obs where device_id = ${deviceId} and source = 'metar' and pres is not null
         and obs_at > ${first}::timestamptz - interval '2 hours' order by obs_at desc limit 1) as q`) as { p: string | null; t: string | null; q: string | null }[]
  const parse = (v: string | null) => (v ? v.slice(1, -1).split(',').map(Number) : null)
  const p0 = parse(prev?.p ?? null), t0 = parse(prev?.t ?? null), q0 = parse(prev?.q ?? null)
  let lastP = p0 ? { at: p0[0]!, v: p0[1]! } : null
  let lastT = t0 ? { at: t0[0]!, t: t0[1]!, h: Number.isFinite(t0[2]) ? t0[2]! : null } : null
  const qnh = q0 ? q0[1]! : null

  const log: { ts: string; field: string; value: string; reason: string }[] = []
  const order = ts.map((_, i) => i).sort((a, b) => (ts[a]! < ts[b]! ? -1 : 1))
  for (const i of order) {
    const at = new Date(ts[i]!).getTime() / 1000
    const pv = pres[i]
    if (pv != null) {
      let reason: string | null = null
      if (qnh != null && Math.abs(pv - qnh) > PRES_VS_QNH) reason = `${pv.toFixed(1)} hPa, a mais de ${PRES_VS_QNH} hPa do QNH ${qnh}`
      else if (qnh == null && lastP && at - lastP.at <= WINDOW_S && Math.abs(pv - lastP.v) > PRES_JUMP) reason = `salto de ${(pv - lastP.v).toFixed(1)} hPa em ${Math.round((at - lastP.at) / 60)} min`
      if (reason) {
        log.push({ ts: ts[i]!, field: 'pres', value: JSON.stringify({ pres_slp: pv, pres_raw: presRaw[i] }), reason })
        pres[i] = null
        presRaw[i] = null
      } else lastP = { at, v: pv }
    }
    const tv = temp[i], hv = hum[i]
    if (tv != null && lastT && at - lastT.at <= WINDOW_S) {
      const dT = tv - lastT.t, dH = hv != null && lastT.h != null ? hv - lastT.h : 0
      if (Math.abs(dT) > TEMP_JUMP || Math.abs(dH) > HUM_JUMP) {
        log.push({ ts: ts[i]!, field: 'aht', value: JSON.stringify({ temp: tv, hum: hv, dew: dew[i] }), reason: `salto de ${dT.toFixed(1)} °C / ${dH.toFixed(0)} pontos` })
        temp[i] = null
        hum[i] = null
        dew[i] = null
        continue
      }
    }
    if (tv != null) lastT = { at, t: tv, h: hv ?? lastT?.h ?? null }
  }
  if (log.length) {
    await sql`
      insert into rejected_readings (device_id, ts, field, value, reason)
      select ${deviceId}, x.ts, x.field, x.value, x.reason
      from unnest(${log.map((l) => l.ts)}::timestamptz[], ${log.map((l) => l.field)}::text[], ${log.map((l) => l.value)}::jsonb[], ${log.map((l) => l.reason)}::text[]) as x(ts, field, value, reason)
      on conflict do nothing`
  }
  return log.length
}
