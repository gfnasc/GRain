import { createHash, timingSafeEqual } from 'node:crypto'
import type { H3Event } from 'h3'

export function hashDeviceToken(token: string) {
  return createHash('sha256').update(token).digest('hex')
}

// Valida "Authorization: Bearer <token>" e devolve o id da estação.
export async function requireDevice(event: H3Event): Promise<string> {
  const header = getHeader(event, 'authorization') ?? ''
  const match = header.match(/^Bearer\s+(\S+)$/i)
  if (!match) throw createError({ statusCode: 401, message: 'token ausente' })

  const hash = hashDeviceToken(match[1]!)
  const sql = useSql()
  const rows = (await sql`select id, token_hash from devices where token_hash = ${hash}`) as {
    id: string
    token_hash: string
  }[]
  const row = rows[0]
  if (!row || !timingSafeEqual(Buffer.from(row.token_hash), Buffer.from(hash))) {
    throw createError({ statusCode: 401, message: 'token inválido' })
  }
  return row.id
}
