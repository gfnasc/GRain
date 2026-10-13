// Chuva nas próximas 1 h, 2 h e 3 h: três métodos, do menos ao mais preciso, calculados a
// cada 10 min (a cada envio da estação), guardados e verificados depois contra o que
// aconteceu (METAR do aeroporto, botão "Está chovendo aqui" e, sem METAR, o modelo).
//   M1  barômetro da estação: queda da pressão em 1 h e 3 h (descontada a maré diária) e nível
//   M2  M1 + aeroporto: chuva/trovoada observada ou nas proximidades, nuvens CB/TCU,
//       ar perto de saturar, teto baixo, rajadas e TAF (previsão do aeroporto)
//   M3  modelo numérico (Open-Meteo): probabilidade por hora, chuva em blocos de 15 min, CAPE
// Os pesos são fixos por enquanto; a fase 18 ajusta pelo histórico (M4).

export type Method = 'm1' | 'm2' | 'm3'
export const METHODS: Method[] = ['m1', 'm2', 'm3']
export const HORIZONS = [1, 2, 3] as const
type Horizon = (typeof HORIZONS)[number]

type Pred = { method: Method; horizon: Horizon; prob: number; reason: string; features: Record<string, unknown> }

const sigmoid = (x: number) => 1 / (1 + Math.exp(-x))
const round2 = (v: number) => Math.round(v * 100) / 100
const n1 = (v: number) => v.toLocaleString('pt-BR', { maximumFractionDigits: 1, minimumFractionDigits: 1 })
const pct = (p: number) => `${Math.round(p * 100)} %`
// Chance de chover em algum momento das próximas h horas a partir da chance em 1 h
// (janelas maiores acumulam chance, mas as horas seguidas não são independentes).
const widen = (p1: number, h: Horizon) => 1 - Math.pow(1 - p1, h === 1 ? 1 : h === 2 ? 1.6 : 2.1)

export const levelOf = (p: number) => (p >= 0.6 ? 'alta' : p >= 0.3 ? 'média' : 'baixa')

// --- dados de entrada ---

type PressureTrend = { pres: number | null; d1: number | null; d3: number | null }

async function pressureTrend(deviceId: string): Promise<PressureTrend> {
  const sql = useSql()
  const [r] = (await sql`
    with last as (
      select ts, pres_slp from readings where device_id = ${deviceId} and pres_slp is not null and ts > now() - interval '30 minutes'
      order by ts desc limit 1)
    select l.pres_slp as pres,
      l.pres_slp - (select pres_slp from readings r where r.device_id = ${deviceId} and r.pres_slp is not null
        and r.ts between l.ts - interval '80 minutes' and l.ts - interval '40 minutes'
        order by abs(extract(epoch from r.ts - (l.ts - interval '1 hour'))) limit 1) as d1,
      l.pres_slp - (select pres_slp from readings r where r.device_id = ${deviceId} and r.pres_slp is not null
        and r.ts between l.ts - interval '200 minutes' and l.ts - interval '160 minutes'
        order by abs(extract(epoch from r.ts - (l.ts - interval '3 hours'))) limit 1) as d3
    from last l`) as PressureTrend[]
  return r ?? { pres: null, d1: null, d3: null }
}

type MetarNow = {
  raining: boolean
  nearby: boolean
  cb: boolean
  tcu: boolean
  spread: number | null
  ceiling: number | null // pés
  gust: number | null    // km/h
  wx: string | null
}

async function metarNow(deviceId: string): Promise<MetarNow | null> {
  const sql = useSql()
  const [o] = (await sql`
    select temp, dew, wx, gust_kmh, raw from outdoor_obs
    where device_id = ${deviceId} and source = 'metar' and obs_at > now() - interval '90 minutes'
    order by obs_at desc limit 1`) as { temp: number | null; dew: number | null; wx: string | null; gust_kmh: number | null; raw: string | null }[]
  if (!o) return null
  const raw = o.raw ?? ''
  const layers = [...raw.matchAll(/\b(FEW|SCT|BKN|OVC|VV)(\d{3})(CB|TCU)?\b/g)].map((m) => ({ cover: m[1], base: Number(m[2]) * 100, type: m[3] }))
  const ceil = layers.filter((l) => l.cover === 'BKN' || l.cover === 'OVC' || l.cover === 'VV').map((l) => l.base)
  return {
    raining: metarIsRaining(o.wx),
    nearby: /\bVC(SH|TS)\b/.test(o.wx ?? ''),
    cb: layers.some((l) => l.type === 'CB'),
    tcu: layers.some((l) => l.type === 'TCU'),
    spread: o.temp != null && o.dew != null ? o.temp - o.dew : null,
    ceiling: ceil.length ? Math.min(...ceil) : null,
    gust: o.gust_kmh,
    wx: o.wx,
  }
}

// --- métodos ---

// Maré barométrica: nos trópicos a pressão sobe e desce todo dia (picos perto de 10h e 22h,
// vales perto de 4h e 16h, hora solar), sem relação com chuva. Modelo clássico: onda de 12 h
// (~1,1 hPa) + onda de 24 h (~0,6 hPa). A tendência usada pelos métodos desconta essa variação.
function tideHpa(epochS: number, lon: number): number {
  const h = (((epochS / 3600 + lon / 15) % 24) + 24) % 24
  return 1.1 * Math.cos((2 * Math.PI * (h - 10)) / 12) + 0.6 * Math.cos((2 * Math.PI * (h - 8)) / 24)
}

function detide(p: PressureTrend, nowS: number, lon: number | null): PressureTrend & { tide3: number | null } {
  if (lon == null) return { ...p, tide3: null }
  const t = (back: number) => tideHpa(nowS, lon) - tideHpa(nowS - back, lon)
  return {
    pres: p.pres,
    d1: p.d1 == null ? null : p.d1 - t(3600),
    d3: p.d3 == null ? null : p.d3 - t(3 * 3600),
    tide3: Math.round(t(3 * 3600) * 10) / 10,
  }
}

function m1Score(p: PressureTrend & { tide3?: number | null }): { s: number; why: string[] } | null {
  if (p.pres == null) return null
  let s = -2.2 // ~10 %: chance de base de chover numa hora qualquer
  const why: string[] = []
  if (p.d3 != null) {
    if (p.d3 <= -0.5) {
      s += Math.min(2, -0.6 * p.d3)
      why.push(`pressão caiu ${n1(-p.d3)} hPa em 3 h`)
    } else if (p.d3 >= 2) {
      s -= 0.5
      why.push(`pressão subindo (+${n1(p.d3)} hPa em 3 h)`)
    } else {
      why.push('pressão estável')
    }
  }
  if (p.d1 != null && p.d1 <= -0.5) {
    s += p.d1 <= -1 ? 0.8 : 0.4
    why.push(`queda de ${n1(-p.d1)} hPa na última hora`)
  }
  if (p.pres < 1005) s += 0.8
  else if (p.pres < 1010) s += 0.4
  else if (p.pres > 1020) s -= 0.5
  if (p.pres < 1010 || p.pres > 1020) why.push(`pressão ${p.pres < 1010 ? 'baixa' : 'alta'} (${Math.round(p.pres)} hPa)`)
  if (p.d3 == null) why.push('sem 3 h de dados para a tendência')
  else if (p.tide3 != null && Math.abs(p.tide3) >= 0.5) why.push(`descontada a variação diária normal (${p.tide3 > 0 ? '+' : ''}${n1(p.tide3)} hPa)`)
  return { s, why }
}

function tafBonus(taf: OutdoorTaf | null | undefined, fromS: number, toS: number): { bonus: number; why: string | null } {
  if (!taf?.groups?.length) return { bonus: 0, why: null }
  let best = { bonus: 0, why: null as string | null }
  for (const g of taf.groups) {
    if (!g.wx || !/(RA|DZ|SH|TS)/.test(g.wx) || g.to <= fromS || g.from >= toS) continue
    const bonus = g.prob === 30 || g.change === 'TEMPO' ? 0.5 : g.prob === 40 ? 0.7 : 0.9
    if (bonus > best.bonus) {
      const kind = g.prob ? `PROB${g.prob}` : g.change ?? 'previsão'
      best = { bonus, why: `aeroporto prevê ${g.wx} (${kind})` }
    }
  }
  return best
}

type OutdoorTaf = { at: number; groups: TafGroup[]; raw: string }

function computeAll(nowS: number, raw: PressureTrend, lon: number | null, m: MetarNow | null, taf: OutdoorTaf | null | undefined, outlook: RainOutlook | null | undefined): Pred[] {
  const out: Pred[] = []
  const p = detide(raw, nowS, lon)
  const base = m1Score(p)

  // M1: só o barômetro.
  if (base) {
    for (const h of HORIZONS) {
      out.push({ method: 'm1', horizon: h, prob: round2(widen(sigmoid(base.s), h)), reason: base.why.join(', '), features: { ...p, d1_raw: raw.d1, d3_raw: raw.d3 } })
    }
  }

  // M2: barômetro + aeroporto (precisa de METAR recente).
  if (m) {
    let s = base?.s ?? -2.2
    const why = base ? [...base.why] : []
    if (m.raining) { s += 2.0; why.unshift(`chovendo no aeroporto (${m.wx})`) }
    if (m.nearby) { s += 1.2; why.unshift('chuva ou trovoada nas proximidades do aeroporto') }
    if (m.cb) { s += 1.0; why.push('nuvens de trovoada (CB)') } else if (m.tcu) { s += 0.6; why.push('nuvens de grande desenvolvimento (TCU)') }
    if (m.spread != null) {
      if (m.spread <= 1) { s += 0.9; why.push('ar quase saturado') }
      else if (m.spread <= 2) { s += 0.6; why.push(`ar úmido (orvalho a ${n1(m.spread)} °C da temperatura)`) }
      else if (m.spread >= 8) { s -= 0.6; why.push('ar seco') }
    }
    if (m.ceiling != null && m.ceiling < 1500) { s += 0.4; why.push(`teto baixo (${m.ceiling} pés)`) }
    if (m.gust != null && m.gust >= 46) { s += 0.3; why.push(`rajadas de ${Math.round(m.gust)} km/h`) }
    for (const h of HORIZONS) {
      const t = tafBonus(taf, nowS, nowS + h * 3600)
      const reason = [...why, ...(t.why ? [t.why] : [])].join(', ')
      out.push({ method: 'm2', horizon: h, prob: round2(widen(sigmoid(s + t.bonus), h)), reason, features: { ...p, ...m, taf: t.bonus } })
    }
  }

  // M3: modelo numérico (previsão buscada há no máximo 75 min).
  if (outlook && Date.now() - outlook.at <= 75 * 60_000) {
    for (const h of HORIZONS) {
      const end = nowS + h * 3600
      const hours = outlook.hourly.filter((x) => x.t + 3600 > nowS && x.t < end && x.pp != null)
      if (!hours.length) continue
      const pps = hours.map((x) => x.pp! / 100)
      const pMax = Math.max(...pps)
      const pUnion = 1 - pps.reduce((a, q) => a * (1 - q), 1)
      let prob = 0.7 * pMax + 0.3 * pUnion
      const peak = hours[pps.indexOf(pMax)]!
      const why = [`modelo: ${pct(pMax)} na hora mais provável (${new Date(peak.t * 1000).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: useRuntimeConfig().public.tz as string })})`]
      const mm = outlook.m15.filter((x) => x.t >= nowS - 900 && x.t < end && x.mm != null).reduce((a, x) => a + x.mm!, 0)
      if (mm >= 0.1) { prob = Math.max(prob, 0.7); why.push(`${n1(mm)} mm previstos`) }
      const cape = Math.max(...hours.map((x) => x.cape ?? 0))
      if (cape >= 1000) { prob = Math.min(1, prob + 0.05); why.push(`energia para trovoada (CAPE ${Math.round(cape)})`) }
      out.push({ method: 'm3', horizon: h, prob: round2(Math.min(1, prob)), reason: why.join(', '), features: { pps, mm: round2(mm), cape } })
    }
  }
  return out
}

// --- cálculo, verificação e leitura ---

export async function computeRain(deviceId: string): Promise<void> {
  const sql = useSql()
  const nowS = Math.floor(Date.now() / 1000)
  const bucket = new Date((nowS - (nowS % 600)) * 1000).toISOString()
  const done = (await sql`select 1 from rain_predictions where device_id = ${deviceId} and issued_at = ${bucket} limit 1`) as unknown[]
  if (done.length) return
  const [d] = (await sql`select site, outdoor from devices where id = ${deviceId}`) as { site: Site | null; outdoor: { taf?: OutdoorTaf | null; rain?: RainOutlook | null } | null }[]
  const [p, m] = await Promise.all([pressureTrend(deviceId), metarNow(deviceId)])
  const preds = computeAll(nowS, p, d?.site?.lon ?? null, m, d?.outdoor?.taf, d?.outdoor?.rain)
  if (!preds.length) return
  await sql`
    insert into rain_predictions (device_id, issued_at, method, horizon_h, prob, reason, features)
    select ${deviceId}, ${bucket}, x.method, x.horizon_h, x.prob, x.reason, x.features
    from unnest(${preds.map((q) => q.method)}::text[], ${preds.map((q) => q.horizon)}::smallint[], ${preds.map((q) => q.prob)}::real[],
                ${preds.map((q) => q.reason)}::text[], ${preds.map((q) => JSON.stringify(q.features))}::jsonb[])
      as x(method, horizon_h, prob, reason, features)
    on conflict do nothing`
}

// Choveu na janela [emissão, emissão + h)? Sim: METAR com chuva (fora "nas proximidades") ou
// botão "Está chovendo aqui". Não: METARs suficientes sem chuva. Sem METAR: o modelo decide
// (≥ 0,1 mm). Sem nada: fica sem verificação.
export async function verifyRain(deviceId: string): Promise<number> {
  const sql = useSql()
  const rows = (await sql`
    update rain_predictions p set outcome = v.rained, verified_at = now()
    from (
      select k.issued_at, k.horizon_h,
        case
          when exists (select 1 from rain_reports r where r.device_id = ${deviceId} and r.ts >= k.issued_at and r.ts < k.win_end)
            or exists (select 1 from outdoor_obs o where o.device_id = ${deviceId} and o.source = 'metar'
                       and o.obs_at >= k.issued_at and o.obs_at < k.win_end and o.wx ~ ${METAR_RAIN_SQL})
            then true
          when (select count(*) from outdoor_obs o where o.device_id = ${deviceId} and o.source = 'metar'
                and o.obs_at >= k.issued_at and o.obs_at < k.win_end) >= k.horizon_h
            then false
          when exists (select 1 from outdoor_obs o where o.device_id = ${deviceId} and o.source = 'open-meteo'
                       and o.obs_at > k.issued_at and o.obs_at <= k.win_end and o.precip is not null)
            then exists (select 1 from outdoor_obs o where o.device_id = ${deviceId} and o.source = 'open-meteo'
                         and o.obs_at > k.issued_at and o.obs_at <= k.win_end and o.precip >= 0.1)
          else null
        end as rained
      from (
        select distinct issued_at, horizon_h, issued_at + make_interval(hours => horizon_h::int) as win_end
        from rain_predictions
        where device_id = ${deviceId} and outcome is null and issued_at > now() - interval '3 days'
          and issued_at + make_interval(hours => horizon_h::int) < now() - interval '20 minutes'
      ) k
    ) v
    where p.device_id = ${deviceId} and p.issued_at = v.issued_at and p.horizon_h = v.horizon_h and v.rained is not null
    returning 1`) as unknown[]
  return rows.length
}

export type Score = {
  method: Method
  horizon: number
  n: number
  events: number
  brier: number | null
  skill: number | null       // 1 − Brier / Brier da climatologia (> 0 é melhor que "chutar a frequência")
  hits: number
  false_alarms: number
  misses: number
  reliability: { bin: number; n: number; freq: number }[]  // faixas de 20 %
}

export const MIN_SCORE_N = 100
export const MIN_SCORE_EVENTS = 5

let scoreCache: { at: number; deviceId: string; rows: Score[] } | null = null

export async function rainScores(deviceId: string, days = 30): Promise<Score[]> {
  if (scoreCache && scoreCache.deviceId === deviceId && days === 30 && Date.now() - scoreCache.at < 10 * 60_000) return scoreCache.rows
  const sql = useSql()
  const since = new Date(Date.now() - days * 86400_000).toISOString()
  const agg = (await sql`
    select method, horizon_h, count(*)::int as n, sum(outcome::int)::int as events,
      avg(power(prob - outcome::int, 2)) as brier,
      count(*) filter (where prob >= 0.5 and outcome)::int as hits,
      count(*) filter (where prob >= 0.5 and not outcome)::int as false_alarms,
      count(*) filter (where prob < 0.5 and outcome)::int as misses
    from rain_predictions where device_id = ${deviceId} and outcome is not null and issued_at > ${since}
    group by 1, 2 order by 1, 2`) as any[]
  const bins = (await sql`
    select method, horizon_h, least(width_bucket(prob, 0, 1, 5), 5) as bin, count(*)::int as n, avg(outcome::int) as freq
    from rain_predictions where device_id = ${deviceId} and outcome is not null and issued_at > ${since}
    group by 1, 2, 3 order by 1, 2, 3`) as any[]
  const rows: Score[] = agg.map((r) => {
    const f = r.n ? r.events / r.n : 0
    const ref = f * (1 - f)
    const b = r.brier == null ? null : Number(r.brier)
    return {
      method: r.method,
      horizon: r.horizon_h,
      n: r.n,
      events: r.events,
      brier: b == null ? null : Math.round(b * 1000) / 1000,
      skill: b != null && ref > 0 ? Math.round((1 - b / ref) * 100) / 100 : null,
      hits: r.hits,
      false_alarms: r.false_alarms,
      misses: r.misses,
      reliability: bins.filter((b) => b.method === r.method && b.horizon_h === r.horizon_h).map((b) => ({ bin: b.bin, n: b.n, freq: Math.round(Number(b.freq) * 100) / 100 })),
    }
  })
  if (days === 30) scoreCache = { at: Date.now(), deviceId, rows }
  return rows
}

// Método mostrado em destaque para cada janela: o de menor Brier quando o placar tem dados
// suficientes para todos; antes disso, o modelo (M3), depois M2 e M1, conforme houver.
function bestMethod(available: Method[], scores: Score[], h: number): Method {
  const s = scores.filter((x) => x.horizon === h && available.includes(x.method) && x.n >= MIN_SCORE_N && x.events >= MIN_SCORE_EVENTS && x.brier != null)
  if (s.length === available.length && s.length > 1) return s.sort((a, b) => a.brier! - b.brier!)[0]!.method
  return (['m3', 'm2', 'm1'] as Method[]).find((m) => available.includes(m))!
}

export type RainNow = {
  issued_at: string
  methods: Partial<Record<Method, { prob: number; reason: string }[]>>  // índice 0..2 = 1 h, 2 h, 3 h
  best: { method: Method; prob: number; level: string; reason: string }[]
  chosen_by: 'placar' | 'padrão'
}

export async function latestRain(deviceId: string): Promise<RainNow | null> {
  const sql = useSql()
  const rows = (await sql`
    select issued_at, method, horizon_h, prob, reason from rain_predictions
    where device_id = ${deviceId} and issued_at = (
      select max(issued_at) from rain_predictions where device_id = ${deviceId} and issued_at > now() - interval '40 minutes')
    order by method, horizon_h`) as { issued_at: string; method: Method; horizon_h: number; prob: number; reason: string }[]
  if (!rows.length) return null
  const methods: RainNow['methods'] = {}
  for (const r of rows) (methods[r.method] ??= [])[r.horizon_h - 1] = { prob: r.prob, reason: r.reason }
  const available = METHODS.filter((m) => methods[m]?.length === 3)
  if (!available.length) return null
  const scores = await rainScores(deviceId)
  let byScore = false
  const best = HORIZONS.map((h) => {
    const m = bestMethod(available, scores, h)
    if (scores.some((x) => x.method === m && x.horizon === h && x.n >= MIN_SCORE_N)) byScore = true
    const v = methods[m]![h - 1]!
    return { method: m, prob: v.prob, level: levelOf(v.prob), reason: v.reason }
  })
  return { issued_at: new Date(rows[0]!.issued_at).toISOString(), methods, best, chosen_by: byScore ? 'placar' : 'padrão' }
}

// Versão compacta para a estação (resposta do envio): porcentagens, método e motivo curto.
export function rainForDevice(r: RainNow | null) {
  if (!r) return undefined
  const top = r.best.reduce((a, b) => (b.prob > a.prob ? b : a))
  return {
    at: Math.floor(new Date(r.issued_at).getTime() / 1000),
    p: r.best.map((b) => Math.round(b.prob * 100)),
    m: r.best.map((b) => b.method.toUpperCase()),
    r: top.reason.length > 90 ? top.reason.slice(0, 89) + '…' : top.reason,
  }
}

export async function reportRain(deviceId: string, via: 'painel' | 'estacao'): Promise<string> {
  const sql = useSql()
  const nowS = Math.floor(Date.now() / 1000)
  const ts = new Date((nowS - (nowS % 600)) * 1000).toISOString()
  await sql`insert into rain_reports (device_id, ts, via) values (${deviceId}, ${ts}, ${via}) on conflict do nothing`
  return ts
}
