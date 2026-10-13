// Os dois testes longos, que começaram juntos com a gravação do firmware 0.14.0:
//   robustez (fase 17, 72 h): contado desde a gravação do firmware atual (outra gravação
//     recomeça a contagem) ou desde o início oficial marcado para ele (trials 'robustez'). Passa com 72 h sem reinícios, sem buracos de mais de 15 min no
//     histórico e com a memória estável.
//   chuva (fase 18, 30 dias): começa na primeira mensagem do firmware 0.14.0 (ou mais novo), ou
//     no início oficial marcado (pnpm trial:start), e não recomeça com gravações. Passa com 30 dias de previsões do M2T e do M4 em pelo menos 90 %
//     das rodadas (falha só quando as perdidas passam de 10 % do total do teste); o placar diz qual método ganhou (com pelo menos 5 chuvas conferidas).

export const HEALTH_TRIAL_H = 72
export const RAIN_TRIAL_DAYS = 30
export const RAIN_TRIAL_FW = '0.14.0'
const RAIN_MIN_COVERAGE = 90

const ver = (v: string) => v.split(/[.-]/).slice(0, 3).map((x) => Number.parseInt(x, 10) || 0)
function atLeast(v: string, min: string) {
  const a = ver(v), b = ver(min)
  for (let i = 0; i < 3; i++) if (a[i] !== b[i]) return a[i]! > b[i]!
  return true
}

// Chamado a cada envio: a primeira mensagem do firmware que inicia o teste da chuva marca o começo.
export async function startTrials(deviceId: string, fw: string) {
  if (!atLeast(fw, RAIN_TRIAL_FW)) return
  await useSql()`insert into trials (device_id, key, started_at, fw) values (${deviceId}, 'chuva', now(), ${fw}) on conflict do nothing`
}

export type TrialState = 'andamento' | 'falhou' | 'sucesso' | 'aguardando'

// Saúde da estação nas primeiras `hours` horas do firmware atual (o teste): depois desse
// prazo o resultado fica congelado.
export async function healthReport(deviceId: string, hours = HEALTH_TRIAL_H) {
  const sql = useSql()
  // Começo do teste: primeiro registro com o firmware atual (gravações reiniciam a contagem),
  // ou o início oficial marcado para esse firmware (pnpm trial:start, depois do teste geral).
  const [start] = (await sql`
    with cur as (select fw from status_log where device_id = ${deviceId} order by ts desc limit 1)
    select greatest(
      (select min(ts) from status_log, cur where device_id = ${deviceId} and status_log.fw = cur.fw),
      (select started_at from trials, cur where device_id = ${deviceId} and key = 'robustez' and trials.fw = cur.fw)) as ts`) as { ts: string | null }[]
  const startMs = start?.ts ? new Date(start.ts).getTime() : Date.now()
  const untilMs = Math.min(Date.now(), startMs + hours * 3600_000)
  const since = new Date(startMs).toISOString()
  const until = new Date(untilMs).toISOString()

  const log = (await sql`
    select ts, fw, uptime_s, heap, heap_min, rssi, queue, boots, reset from status_log
    where device_id = ${deviceId} and ts >= ${since} and ts <= ${until} order by ts`) as any[]

  // Reinícios: o contador da estação subiu, ou o tempo ligado voltou para trás.
  const restarts: { ts: string; reason: string | null }[] = []
  for (let i = 1; i < log.length; i++) {
    const a = log[i - 1], b = log[i]
    const byBoots = a.boots != null && b.boots != null && b.boots > a.boots
    const byUptime = a.uptime_s != null && b.uptime_s != null && b.uptime_s < a.uptime_s
    if (byBoots || byUptime) restarts.push({ ts: new Date(b.ts).toISOString(), reason: b.reset })
  }

  // Buracos: intervalos de mais de 15 min entre médias recebidas (inclui o tempo desde a última).
  const gaps = (await sql`
    with r as (select ts, lead(ts, 1, ${until}::timestamptz) over (order by ts) as nxt from readings where device_id = ${deviceId} and ts >= ${since} and ts <= ${until})
    select ts as from_ts, nxt as to_ts, extract(epoch from nxt - ts)::int as secs from r
    where nxt - ts > interval '15 minutes' order by ts`) as { from_ts: string; to_ts: string; secs: number }[]
  const [cov] = (await sql`
    select count(*)::int as n, min(ts) as first from readings where device_id = ${deviceId} and ts >= ${since} and ts <= ${until}`) as { n: number; first: string | null }[]

  const heaps = log.map((l) => l.heap_min).filter((v) => v != null) as number[]
  const rssi = log.map((l) => l.rssi).filter((v) => v != null) as number[]
  const last = log[log.length - 1] ?? null
  const first = log[0] ?? null
  const spanH = first ? (untilMs - new Date(first.ts).getTime()) / 3600_000 : 0
  const expected = Math.floor((untilMs - new Date(cov?.first ?? untilMs).getTime()) / 300_000)
  // Memória: a menor livre cai logo depois de ligar (conexões seguras com a nuvem); a referência
  // é a medida 30 min depois do começo. Estável = não caiu mais de 10 % e nunca abaixo de 40 KB.
  const ref = log.find((l) => l.heap_min != null && new Date(l.ts).getTime() >= startMs + 30 * 60_000)?.heap_min as number | undefined
  const heapMin = heaps.length ? { first: ref ?? null, last: heaps[heaps.length - 1]!, lowest: Math.min(...heaps) } : null
  const memOk = !heapMin || (heapMin.lowest > 40_000 && (heapMin.first == null || heapMin.last >= heapMin.first * 0.9))
  const endsAt = start?.ts ? startMs + hours * 3600_000 : null
  const failed = restarts.length > 0 || gaps.length > 0 || !memOk
  const state: TrialState = !endsAt ? 'aguardando' : failed ? 'falhou' : Date.now() >= endsAt ? 'sucesso' : 'andamento'
  return {
    hours,
    since,
    started_at: start?.ts ? since : null,
    ends_at: endsAt ? new Date(endsAt).toISOString() : null,
    state,
    observed_h: Math.round(spanH * 10) / 10,
    samples: log.length,
    fw: last?.fw ?? null,
    uptime_s: last?.uptime_s ?? null,
    last_reset: last?.reset ?? null,
    boots: last?.boots ?? null,
    restarts,
    gaps: gaps.map((g) => ({ from: new Date(g.from_ts).toISOString(), to: new Date(g.to_ts).toISOString(), minutes: Math.round(g.secs / 60) })),
    coverage: expected > 0 ? Math.min(100, Math.round(((cov?.n ?? 0) / expected) * 100)) : null,
    heap_min: heapMin,
    mem_ok: memOk,
    heap_now: last?.heap ?? null,
    rssi: rssi.length ? { avg: Math.round(rssi.reduce((a, b) => a + b, 0) / rssi.length), min: Math.min(...rssi) } : null,
  }
}

// Teste da chuva: rodadas com M2T e M4, chuvas conferidas e placar desde o começo.
export async function rainTrial(deviceId: string) {
  const sql = useSql()
  const [t] = (await sql`select started_at, fw from trials where device_id = ${deviceId} and key = 'chuva'`) as { started_at: string; fw: string }[]
  if (!t) return { started_at: null, ends_at: null, fw_needed: RAIN_TRIAL_FW, state: 'aguardando' as TrialState, days: RAIN_TRIAL_DAYS }
  const start = new Date(t.started_at)
  const endsAt = new Date(start.getTime() + RAIN_TRIAL_DAYS * 86400_000)
  const until = new Date(Math.min(Date.now(), endsAt.getTime()))
  const [c] = (await sql`
    select count(distinct issued_at) filter (where method = 'm2t')::int as m2t,
           count(distinct issued_at) filter (where method = 'm4')::int as m4,
           count(distinct issued_at) filter (where horizon_h = 1 and outcome)::int as rain_rounds,
           count(distinct date_trunc('hour', issued_at)) filter (where horizon_h = 1 and outcome)::int as rain_hours
    from rain_predictions where device_id = ${deviceId} and issued_at >= ${start.toISOString()} and issued_at < ${until.toISOString()}`) as {
    m2t: number; m4: number; rain_rounds: number; rain_hours: number
  }[]
  // Cobertura: o teste tem 30 × 144 rodadas e pode perder até 10 % delas (≈ 72 h fora do ar, somando
  // tudo). Só falha quando as perdidas passam dessa folga; antes disso, uma queda ainda se recupera.
  // A rodada em curso pode ainda não ter sido calculada: não conta como perdida.
  const total = RAIN_TRIAL_DAYS * 144
  const budget = Math.floor((total * (100 - RAIN_MIN_COVERAGE)) / 100)
  const expected = Math.max(1, Math.floor((until.getTime() - start.getTime()) / 600_000))
  const pctOf = (n: number) => Math.min(100, Math.round((n / expected) * 100))
  const coverage = { m2t: pctOf(c?.m2t ?? 0), m4: pctOf(c?.m4 ?? 0) }
  const missed = Math.max(0, expected - 1 - Math.min(c?.m2t ?? 0, c?.m4 ?? 0))
  const failed = missed > budget
  const done = Date.now() >= endsAt.getTime()
  const state: TrialState = failed ? 'falhou' : done ? 'sucesso' : 'andamento'
  return {
    started_at: start.toISOString(),
    ends_at: endsAt.toISOString(),
    fw: t.fw,
    state,
    days: RAIN_TRIAL_DAYS,
    min_coverage: RAIN_MIN_COVERAGE,
    coverage,
    missed,
    budget,
    rain_hours: c?.rain_hours ?? 0,
    min_events: MIN_SCORE_EVENTS,
  }
}

// Versão compacta para a estação (resposta do envio): fim previsto (epoch UTC) e situação.
export async function trialsForDevice(deviceId: string) {
  const [h, r] = await Promise.all([healthReport(deviceId), rainTrial(deviceId)])
  const ep = (iso: string | null) => (iso ? Math.floor(new Date(iso).getTime() / 1000) : null)
  return {
    h72: { end: ep(h.ends_at), state: h.state },
    d30: { end: ep(r.ends_at), state: r.state },
  }
}
