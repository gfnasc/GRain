import type { H3Event } from 'h3'

// Intervalo pedido pelo painel:
//   ?range=24h | 7d | 30d                         (relativo a agora)
//   ?from=AAAA-MM-DD&to=AAAA-MM-DD                (dias inteiros no fuso configurado)
const RELATIVE: Record<string, number> = { '24h': 86400, '7d': 7 * 86400, '30d': 30 * 86400 }
const MAX_DAYS = 400
const DATE = /^\d{4}-\d{2}-\d{2}$/

export type ResolvedRange = { from: Date; to: Date; label: string }

export async function resolveRange(event: H3Event): Promise<ResolvedRange> {
  const q = getQuery(event)
  const tz = useRuntimeConfig(event).public.tz as string

  if (typeof q.from === 'string' || typeof q.to === 'string') {
    const from = String(q.from ?? '')
    const to = String(q.to ?? '')
    if (!DATE.test(from) || !DATE.test(to)) {
      throw createError({ statusCode: 400, message: 'use from e to no formato AAAA-MM-DD' })
    }
    if (from > to) throw createError({ statusCode: 400, message: 'from deve ser anterior a to' })
    const sql = useSql()
    // Converte os dias locais em instantes UTC (do início de "from" ao fim de "to").
    const [b] = (await sql`
      select (${from}::date::timestamp at time zone ${tz}) as f,
             ((${to}::date + 1)::timestamp at time zone ${tz}) as t`) as { f: string; t: string }[]
    const range = { from: new Date(b!.f), to: new Date(b!.t), label: `${from} a ${to}` }
    if (range.to.getTime() - range.from.getTime() > MAX_DAYS * 86400_000) {
      throw createError({ statusCode: 400, message: `intervalo máximo de ${MAX_DAYS} dias` })
    }
    return range
  }

  const key = typeof q.range === 'string' ? q.range : '24h'
  const seconds = RELATIVE[key]
  if (!seconds) throw createError({ statusCode: 400, message: 'range deve ser 24h, 7d ou 30d' })
  const to = new Date()
  return { from: new Date(to.getTime() - seconds * 1000), to, label: key }
}

// Resolução dos gráficos conforme o tamanho do intervalo.
export function stepFor(range: ResolvedRange): number {
  const span = (range.to.getTime() - range.from.getTime()) / 1000
  if (span <= 2 * 86400) return 600 // médias de 10 min, como chegam
  if (span <= 10 * 86400) return 3600
  if (span <= 45 * 86400) return 3 * 3600
  return 86400
}
