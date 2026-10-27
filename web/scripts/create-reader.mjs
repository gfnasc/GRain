// Cria (ou troca o token de) um leitor: aparelho que só lê o painel, como a Gspresso.
// Uso: pnpm reader:token <nome>     (ex.: pnpm reader:token gspresso)
// O token é mostrado UMA vez, no arquivo .reader-token-<nome>.txt (fora do git);
// no banco fica só o hash. Vale só para as rotas GET de leitura (Authorization: Bearer).
import { createHash, randomBytes } from 'node:crypto'
import { writeFileSync } from 'node:fs'
import { neon } from '@neondatabase/serverless'

const [id] = process.argv.slice(2)
if (!id || !/^[a-z0-9-]{3,40}$/.test(id)) {
  console.error('Uso: pnpm reader:token <nome>   (ex.: gspresso)')
  process.exit(1)
}
const url = process.env.DATABASE_URL
if (!url) {
  console.error('DATABASE_URL não definida. Rode "vercel env pull .env.local" dentro de web/.')
  process.exit(1)
}

const token = randomBytes(24).toString('base64url')
const hash = createHash('sha256').update(token).digest('hex')
const sql = neon(url)
await sql`
  insert into readers (id, token_hash) values (${id}, ${hash})
  on conflict (id) do update set token_hash = excluded.token_hash, last_used_at = null`

const file = new URL(`../.reader-token-${id}.txt`, import.meta.url)
writeFileSync(file, token + '\n', { mode: 0o600 })
console.log(`Leitor "${id}" cadastrado. Token gravado em web/.reader-token-${id}.txt`)
