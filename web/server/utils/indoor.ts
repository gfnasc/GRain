// Indoor: o ambiente onde a estação está (escritório), cruzado com o Outdoor.
// Conforto (faixas de temperatura e umidade), ar seco, risco de mofo (horas de umidade alta),
// sugestão de abrir ou fechar a janela e atraso térmico (quanto o ambiente demora para
// acompanhar a temperatura de fora e quanto amortece a variação). Só medições fora da sacada.

const NOT_SACADA = (alias: string) =>
  `not exists (select 1 from placements p where p.device_id = ${alias}.device_id and p.place = 'sacada' and ${alias}.ts >= p.started_at and (p.ended_at is null or ${alias}.ts < p.ended_at))`

type Level = 'ok' | 'atenção' | 'alerta'

export type IndoorInsights = {
  now: { ts: string; temp: number | null; hum: number | null; dew: number | null } | null
  comfort: { label: string; level: Level; temp: string; hum: string } | null
  dry: { level: Level; text: string } | null
  mold: { level: Level; hours_72h: number; text: string } | null
  window: { action: 'abrir' | 'fechar' | 'tanto faz'; reason: string } | null
  lag: { hours: number; follow: number; pairs: number; text: string } | null
  lag_needed_h: number | null // horas de dados que faltam para o atraso térmico
}

function tempBand(t: number): [string, Level] {
  if (t < 18) return ['frio', 'alerta']
  if (t < 20) return ['fresco', 'atenção']
  if (t <= 26) return ['agradável', 'ok']
  if (t <= 28) return ['quente', 'atenção']
  return ['muito quente', 'alerta']
}
function humBand(h: number): [string, Level] {
  if (h < 30) return ['muito seco', 'alerta']
  if (h < 40) return ['seco', 'atenção']
  if (h <= 60) return ['ideal', 'ok']
  if (h <= 70) return ['úmido', 'atenção']
  return ['muito úmido', 'alerta']
}
const worst = (a: Level, b: Level): Level => (a === 'alerta' || b === 'alerta' ? 'alerta' : a === 'atenção' || b === 'atenção' ? 'atenção' : 'ok')
const n1 = (v: number) => v.toLocaleString('pt-BR', { minimumFractionDigits: 1, maximumFractionDigits: 1 })

// Correlação de Pearson.
function corr(a: number[], b: number[]): number {
  const n = a.length
  const ma = a.reduce((s, x) => s + x, 0) / n, mb = b.reduce((s, x) => s + x, 0) / n
  let sab = 0, saa = 0, sbb = 0
  for (let i = 0; i < n; i++) {
    sab += (a[i]! - ma) * (b[i]! - mb)
    saa += (a[i]! - ma) ** 2
    sbb += (b[i]! - mb) ** 2
  }
  return saa && sbb ? sab / Math.sqrt(saa * sbb) : 0
}

const MIN_LAG_PAIRS = 24 // horas com dado dentro e fora

export async function indoorInsights(deviceId: string): Promise<IndoorInsights> {
  const sql = useSql()
  const [last] = (await sql.query(
    `select ts, temp, hum, dew from readings r where device_id = $1 and ts > now() - interval '30 minutes' and ${NOT_SACADA('r')} order by ts desc limit 1`,
    [deviceId],
  )) as { ts: string; temp: number | null; hum: number | null; dew: number | null }[]

  const out: IndoorInsights = { now: null, comfort: null, dry: null, mold: null, window: null, lag: null, lag_needed_h: null }
  if (last) out.now = { ts: new Date(last.ts).toISOString(), temp: last.temp, hum: last.hum, dew: last.dew }

  // Conforto agora.
  if (last?.temp != null && last.hum != null) {
    const [tl, tv] = tempBand(last.temp)
    const [hl, hv] = humBand(last.hum)
    const level = worst(tv, hv)
    out.comfort = { label: level === 'ok' ? 'confortável' : `${tl}, ${hl}`, level, temp: tl, hum: hl }
    if (last.hum < 40) {
      out.dry = {
        level: last.hum < 30 ? 'alerta' : 'atenção',
        text: last.hum < 30
          ? `Ar muito seco (${Math.round(last.hum)} %): beba água, use umidificador ou uma bacia com água e evite ventilador direto.`
          : `Ar seco (${Math.round(last.hum)} %): vale um umidificador ou uma toalha úmida no ambiente.`,
      }
    }
  }

  // Risco de mofo: horas com umidade ≥ 70 % nos últimos 3 dias (médias de 5 ou 10 min).
  // Cada média vale até a seguinte (no máximo 10 min): funciona com médias de 5 e de 10 min.
  const [m] = (await sql.query(
    `with r as (
       select ts, hum, least(coalesce(extract(epoch from lead(ts) over (order by ts) - ts), 300), 600) / 60 as minutes
       from readings r where device_id = $1 and ts > now() - interval '72 hours' and hum is not null and ${NOT_SACADA('r')})
     select coalesce(sum(minutes) filter (where hum >= 70), 0)::int as minutes, count(*)::int as n from r`,
    [deviceId],
  )) as { minutes: number; n: number }[]
  if (m && m.n) {
    const h = Math.round(m.minutes / 60)
    const level: Level = h >= 36 ? 'alerta' : h >= 12 ? 'atenção' : 'ok'
    out.mold = {
      level,
      hours_72h: h,
      text: level === 'ok'
        ? h ? `Umidade alta só ${h} h nos últimos 3 dias: risco baixo de mofo.` : 'Sem umidade alta nos últimos 3 dias: risco baixo de mofo.'
        : `Umidade acima de 70 % por ${h} h nos últimos 3 dias: ${level === 'alerta' ? 'risco alto' : 'atenção'} para mofo. Ventile o ambiente e evite secar roupa dentro.`,
    }
  }

  // Janela: compara com o Outdoor (e com a chuva prevista).
  if (last?.temp != null && last.hum != null) {
    const o = await latestOutdoor(deviceId)
    const rain = await latestRain(deviceId)
    const now = o.now
    const [w] = (await sql`
      select wx from outdoor_obs o where device_id = ${deviceId} and source = 'metar' and obs_at > now() - interval '90 minutes'
        and not exists (select 1 from rain_reports n where n.device_id = o.device_id and not n.raining and (case when n.covers_from is null then abs(extract(epoch from n.ts - o.obs_at)) <= 1800 else o.obs_at >= n.covers_from and o.obs_at < n.ts + interval '10 minutes' end))
      order by obs_at desc limit 1`) as { wx: string | null }[]
    const raining = metarIsRaining(w?.wx ?? null)
    const rain1h = rain?.best[0]?.prob ?? 0
    if (!now || now.temp == null) {
      out.window = null
    } else {
      const ti = last.temp, to = now.temp
      const di = last.dew ?? ti, dout = now.dew ?? to
      const reasons: string[] = []
      let action: 'abrir' | 'fechar' | 'tanto faz' = 'tanto faz'
      if (raining || rain1h >= 0.6) {
        action = 'fechar'
        reasons.push(raining ? 'está chovendo lá fora' : `chance de chuva de ${Math.round(rain1h * 100)} % na próxima hora`)
      } else if (ti > 25 && to <= ti - 2 && dout <= di + 1) {
        action = 'abrir'
        reasons.push(`lá fora está ${n1(ti - to)} °C mais fresco e o ar não é mais úmido`)
      } else if (last.hum > 65 && dout < di - 1 && to <= ti + 2) {
        action = 'abrir'
        reasons.push('o ar de fora é mais seco e ajuda a tirar a umidade')
      } else if (last.hum < 40 && dout > di + 1 && to <= ti + 2) {
        action = 'abrir'
        reasons.push('o ar de fora é mais úmido e ajuda com o ar seco')
      } else if (to >= ti + 1 && ti >= 20) {
        action = 'fechar'
        reasons.push(`lá fora está ${n1(to - ti)} °C mais quente: fechada, a janela segura o fresco`)
      } else if (ti < 20 && to < ti) {
        action = 'fechar'
        reasons.push(`lá fora está ${n1(ti - to)} °C mais frio`)
      } else {
        reasons.push(`dentro ${n1(ti)} °C, fora ${n1(to)} °C: diferença pequena`)
      }
      out.window = { action, reason: reasons.join('; ') }
    }
  }

  // Atraso térmico: correlação entre a temperatura de fora (hora a hora) e a de dentro deslocada
  // de 0 a 8 h, nos últimos 7 dias; a inclinação mostra quanto da variação de fora chega dentro.
  const hourly = (await sql.query(
    `with ind as (
       select date_trunc('hour', ts) as h, avg(temp) as tin from readings r
       where device_id = $1 and ts > now() - interval '7 days' and temp is not null and ${NOT_SACADA('r')}
       group by 1),
     outd as (
       select date_trunc('hour', obs_at) as h,
              coalesce(avg(temp) filter (where source = 'metar'), avg(temp) filter (where source = 'open-meteo')) as tout
       from outdoor_obs where device_id = $1 and obs_at > now() - interval '7 days 8 hours' and temp is not null
       group by 1)
     select extract(epoch from coalesce(i.h, o.h))::bigint as h, i.tin, o.tout from ind i full join outd o on o.h = i.h order by 1`,
    [deviceId],
  )) as { h: string; tin: number | null; tout: number | null }[]
  const tin = new Map<number, number>(), tout = new Map<number, number>()
  for (const r of hourly) {
    if (r.tin != null) tin.set(Number(r.h), Number(r.tin))
    if (r.tout != null) tout.set(Number(r.h), Number(r.tout))
  }
  let best: { lag: number; c: number; slope: number; pairs: number } | null = null
  for (let lag = 0; lag <= 8; lag++) {
    const a: number[] = [], b: number[] = []
    for (const [h, vi] of tin) {
      const vo = tout.get(h - lag * 3600)
      if (vo != null) { a.push(vo); b.push(vi) }
    }
    if (a.length < MIN_LAG_PAIRS) continue
    const c = corr(a, b)
    const ma = a.reduce((s, x) => s + x, 0) / a.length, mb = b.reduce((s, x) => s + x, 0) / b.length
    let cov = 0, va = 0
    for (let i = 0; i < a.length; i++) { cov += (a[i]! - ma) * (b[i]! - mb); va += (a[i]! - ma) ** 2 }
    if (!best || c > best.c) best = { lag, c, slope: va ? cov / va : 0, pairs: a.length }
  }
  if (best && best.c > 0.3) {
    const follow = Math.max(0, Math.min(1, best.slope))
    out.lag = {
      hours: best.lag,
      follow: Math.round(follow * 100) / 100,
      pairs: best.pairs,
      text: `O ambiente acompanha a temperatura de fora com cerca de ${best.lag} h de atraso e recebe ${Math.round(follow * 100)} % da variação (o resto o prédio amortece).`,
    }
  } else {
    out.lag_needed_h = Math.max(0, MIN_LAG_PAIRS - tin.size)
  }
  return out
}
