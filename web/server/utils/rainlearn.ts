// Métodos treinados (fase 18): M2T (estação + aeroporto) e M4 (estação + aeroporto + modelo).
// Regressão logística sobre os termos do M1/M2 (ver rain.ts), uma por janela (1, 2 e 3 h),
// treinada com as previsões já conferidas. O ponto de partida (e para onde os pesos puxam
// enquanto há pouca chuva conferida) são os pesos fixos:
//   M2T  igual ao M2: base −2,2 e peso 1 em cada termo
//   M4   média, na escala logit, do M2 e do M3: base −1,1, peso 0,5 em cada termo e 0,5 no modelo
// Cada peso aprendido é puxado para o de partida (prior gaussiano), então com poucos dados o
// método continua perto do original e só muda o que o placar sustentar. Previsões emitidas a
// cada 10 min se sobrepõem: o treino usa uma rodada a cada 20 min, cada uma valendo 1/3 (uma por
// hora, na prática), o que também reduz o tráfego com o banco.
// O treino roda depois do envio (no máximo a cada 3 h) e guarda os pesos em rain_models.

type Learned = 'm2t' | 'm4'
export type Weights = Record<string, number> // 'bias', termos de TERM_KEYS e 'modelo' (M4)

const ROW_WEIGHT = 1 / 3
const PRIOR_PRECISION = 4 // desvio de 0,5 em torno do peso de partida
const TRAIN_EVERY_MS = 3 * 3600_000 - 5 * 60_000
const TRAIN_DAYS = 60

export function priorWeights(method: Learned): Weights {
  const k = method === 'm4' ? 0.5 : 1
  const w: Weights = { bias: BASE_LOGIT * k }
  for (const t of TERM_KEYS) w[t] = k
  if (method === 'm4') w.modelo = 0.5
  return w
}

// Correção de janela: o M2 alarga a chance de 1 h para 2 e 3 h (widen); os treinados mantêm
// essa correção como deslocamento fixo, calculado com os pesos de partida.
function offsetFor(method: Learned, s: number, h: Horizon): number {
  return (method === 'm4' ? 0.5 : 1) * (logit(Math.min(0.999, Math.max(0.001, widen(sigmoid(s), h)))) - s)
}

const clampP = (p: number) => Math.min(0.98, Math.max(0.02, p))
const keysOf = (method: Learned) => ['bias', ...TERM_KEYS, ...(method === 'm4' ? ['modelo'] : [])]

function vectorOf(method: Learned, terms: Pick<Term, 'key' | 'v'>[], p3?: number): number[] {
  const byKey = new Map<string, number>()
  for (const t of terms) byKey.set(t.key, (byKey.get(t.key) ?? 0) + t.v)
  return keysOf(method).map((k) => (k === 'bias' ? 1 : k === 'modelo' ? logit(clampP(p3 ?? 0.1)) : byKey.get(k) ?? 0))
}

export function learnedPredict(method: Learned, w: Weights, terms: Term[], h: Horizon, p3?: number) {
  const keys = keysOf(method)
  const x = vectorOf(method, terms, p3)
  const s = BASE_LOGIT + terms.reduce((a, t) => a + t.v, 0)
  const z = keys.reduce((a, k, i) => a + (w[k] ?? priorWeights(method)[k]!) * x[i]!, 0) + offsetFor(method, s, h)
  // Motivo: os termos pela contribuição com os pesos aprendidos.
  const ranked = terms
    .map((t) => ({ ...t, v: (w[t.key] ?? 1) * t.v }))
    .sort((a, b) => b.v - a.v)
  const features: Record<string, unknown> = { terms: Object.fromEntries(terms.map((t) => [t.key, Math.round(t.v * 100) / 100])), s: Math.round(s * 100) / 100 }
  if (p3 != null) features.p3 = p3
  return { prob: sigmoid(z), ranked, features }
}

export type LearnedModels = { get: (method: Learned, h: number) => Weights; info: Map<string, { n: number; events: number; trained_at: string }> }

export async function loadModels(deviceId: string): Promise<LearnedModels> {
  const rows = (await useSql()`select method, horizon_h, weights, n, events, trained_at from rain_models where device_id = ${deviceId}`) as {
    method: Learned; horizon_h: number; weights: Weights; n: number; events: number; trained_at: string
  }[]
  const map = new Map(rows.map((r) => [`${r.method}:${r.horizon_h}`, r]))
  return {
    get: (method, h) => ({ ...priorWeights(method), ...(map.get(`${method}:${h}`)?.weights ?? {}) }),
    info: new Map(rows.map((r) => [`${r.method}:${r.horizon_h}`, { n: r.n, events: r.events, trained_at: new Date(r.trained_at).toISOString() }])),
  }
}

// Resolve A·x = b (A simétrica positiva, pequena) por eliminação de Gauss com pivô.
function solve(A: number[][], b: number[]): number[] {
  const n = b.length
  const M = A.map((row, i) => [...row, b[i]!])
  for (let c = 0; c < n; c++) {
    let piv = c
    for (let r = c + 1; r < n; r++) if (Math.abs(M[r]![c]!) > Math.abs(M[piv]![c]!)) piv = r
    ;[M[c], M[piv]] = [M[piv]!, M[c]!]
    const d = M[c]![c]!
    for (let r = 0; r < n; r++) {
      if (r === c) continue
      const f = M[r]![c]! / d
      if (f) for (let k = c; k <= n; k++) M[r]![k]! -= f * M[c]![k]!
    }
  }
  return M.map((row, i) => row[n]! / row[i]!)
}

// Regressão logística com prior gaussiano (MAP) por Newton-Raphson.
export function fitLogistic(rows: { x: number[]; off: number; y: number }[], prior: number[]): number[] {
  const n = prior.length
  let w = [...prior]
  for (let it = 0; it < 30; it++) {
    const g = w.map((wi, j) => PRIOR_PRECISION * (wi - prior[j]!))
    const H = prior.map((_, i) => prior.map((__, j) => (i === j ? PRIOR_PRECISION : 0)))
    for (const r of rows) {
      const p = sigmoid(r.x.reduce((a, xi, j) => a + xi * w[j]!, r.off))
      const e = ROW_WEIGHT * (p - r.y), q = ROW_WEIGHT * p * (1 - p)
      for (let i = 0; i < n; i++) {
        if (!r.x[i]) continue
        g[i]! += e * r.x[i]!
        for (let j = 0; j < n; j++) if (r.x[j]) H[i]![j]! += q * r.x[i]! * r.x[j]!
      }
    }
    const step = solve(H, g)
    w = w.map((wi, j) => wi - step[j]!)
    if (Math.max(...step.map(Math.abs)) < 1e-6) break
  }
  return w
}

// Refaz os pesos com as previsões conferidas dos últimos 60 dias (no máximo a cada 3 h).
// Os termos são reconstruídos dos dados guardados com o M1 e o M2 (features), e o modelo vem
// do M3 da mesma rodada, então o treino vale também para as rodadas de antes dos treinados.
export async function trainRain(deviceId: string, force = false): Promise<number> {
  const sql = useSql()
  if (!force) {
    const [last] = (await sql`select max(trained_at) as at from rain_models where device_id = ${deviceId}`) as { at: string | null }[]
    if (last?.at && Date.now() - new Date(last.at).getTime() < TRAIN_EVERY_MS) return 0
  }
  const data = (await sql`
    select horizon_h, bool_or(outcome) as y,
      (array_agg(features) filter (where method = 'm1'))[1] as f1,
      (array_agg(features) filter (where method = 'm2'))[1] as f2,
      max(prob) filter (where method = 'm3') as p3
    from rain_predictions
    where device_id = ${deviceId} and outcome is not null and method in ('m1', 'm2', 'm3') and issued_at > now() - make_interval(days => ${TRAIN_DAYS})
      and extract(minute from issued_at)::int % 20 = 0
    group by issued_at, horizon_h`) as { horizon_h: number; y: boolean; f1: any; f2: any; p3: number | null }[]

  const out: { method: Learned; h: number; w: Weights; n: number; events: number }[] = []
  for (const method of ['m2t', 'm4'] as Learned[]) {
    const keys = keysOf(method)
    const prior = keys.map((k) => priorWeights(method)[k]!)
    for (const h of HORIZONS) {
      const rows: { x: number[]; off: number; y: number }[] = []
      for (const d of data) {
        if (d.horizon_h !== h) continue
        if (method === 'm2t' && !d.f2) continue
        if (method === 'm4' && (d.p3 == null || !(d.f1 || d.f2))) continue
        const terms: Pick<Term, 'key' | 'v'>[] = [...(m1Terms(d.f2 ?? d.f1)?.terms ?? []), ...(d.f2 ? airportTerms(d.f2) : [])]
        if (d.f2?.taf) terms.push({ key: 'taf', v: Number(d.f2.taf) })
        const s = BASE_LOGIT + terms.reduce((a, t) => a + t.v, 0)
        rows.push({ x: vectorOf(method, terms, d.p3 ?? undefined), off: offsetFor(method, s, h), y: d.y ? 1 : 0 })
      }
      const w = rows.length ? fitLogistic(rows, prior) : prior
      out.push({ method, h, w: Object.fromEntries(keys.map((k, i) => [k, Math.round(w[i]! * 1000) / 1000])), n: rows.length, events: rows.filter((r) => r.y).length })
    }
  }
  await sql`
    insert into rain_models (device_id, method, horizon_h, weights, n, events, trained_at)
    select ${deviceId}, x.method, x.h, x.w, x.n, x.events, now()
    from unnest(${out.map((o) => o.method)}::text[], ${out.map((o) => o.h)}::smallint[], ${out.map((o) => JSON.stringify(o.w))}::jsonb[],
                ${out.map((o) => o.n)}::int[], ${out.map((o) => o.events)}::int[]) as x(method, h, w, n, events)
    on conflict (device_id, method, horizon_h) do update set weights = excluded.weights, n = excluded.n, events = excluded.events, trained_at = excluded.trained_at`
  return out.length
}

// O que os treinados aprenderam (para a aba Chuva): peso atual × peso de partida em cada termo.
export async function learnedSummary(deviceId: string) {
  const models = await loadModels(deviceId)
  return (['m2t', 'm4'] as Learned[]).map((method) => {
    const prior = priorWeights(method)
    return {
      method,
      horizons: HORIZONS.map((h) => {
        const w = models.get(method, h)
        const info = models.info.get(`${method}:${h}`) ?? null
        return { h, n: info?.n ?? 0, events: info?.events ?? 0, trained_at: info?.trained_at ?? null, weights: keysOf(method).map((k) => ({ key: k, prior: prior[k]!, value: w[k]! })) }
      }),
    }
  })
}
