// Cadastra (ou troca o token de) uma estação e grava o token em um arquivo local.
// Uso: pnpm device:token <device_id> [nome]
//   ex.: pnpm device:token grain-a1b2c3
// O device_id aparece no comando "status"/"config" do console serial do ESP32.
// O token é mostrado UMA vez, no arquivo .device-token-<id>.txt (fora do git);
// no banco fica só o hash.
import { createHash, randomBytes } from 'node:crypto'
import { writeFileSync } from 'node:fs'
import { neon } from '@neondatabase/serverless'

const [id, name = 'GRain'] = process.argv.slice(2)
if (!id || !/^[a-z0-9-]{3,40}$/.test(id)) {
  console.error('Uso: pnpm device:token <device_id> [nome]   (ex.: grain-a1b2c3)')
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
  insert into devices (id, name, token_hash) values (${id}, ${name}, ${hash})
  on conflict (id) do update set token_hash = excluded.token_hash, name = excluded.name`

const file = new URL(`../.device-token-${id}.txt`, import.meta.url)
writeFileSync(file, token + '\n', { mode: 0o600 })
console.log(`Estação "${id}" cadastrada. Token gravado em web/.device-token-${id}.txt`)
console.log('No console serial do ESP32:  nuvem token <conteúdo do arquivo>')
