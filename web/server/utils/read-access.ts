import { timingSafeEqual } from 'node:crypto'
import type { H3Event } from 'h3'

// Rotas de leitura do painel: aceitam a sessão do painel ou o token de um leitor
// ("Authorization: Bearer <token>", tabela readers), como o da Gspresso.
// O token de leitor não vale para nada que altere dados.
export async function requireReadAccess(event: H3Event): Promise<void> {
  const header = getHeader(event, 'authorization') ?? ''
  const match = header.match(/^Bearer\s+(\S+)$/i)
  if (!match) {
    await requireUserSession(event)
    return
  }
  const hash = hashDeviceToken(match[1]!)
  const sql = useSql()
  const rows = (await sql`select id, token_hash from readers where token_hash = ${hash}`) as { id: string; token_hash: string }[]
  const row = rows[0]
  if (!row || !timingSafeEqual(Buffer.from(row.token_hash), Buffer.from(hash))) {
    throw createError({ statusCode: 401, message: 'token de leitura inválido' })
  }
  // Último uso, no máximo uma escrita a cada 10 min.
  await sql`update readers set last_used_at = now()
    where id = ${row.id} and (last_used_at is null or last_used_at < now() - interval '10 minutes')`
}
