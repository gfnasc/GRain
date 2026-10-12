// Aplica server/database/schema.sql no banco de DATABASE_URL.
// Uso: pnpm db:migrate   (lê .env.local)
import { readFileSync } from 'node:fs'
import { neon } from '@neondatabase/serverless'

const url = process.env.DATABASE_URL_UNPOOLED || process.env.DATABASE_URL
if (!url) {
  console.error('DATABASE_URL não definida. Rode "vercel env pull .env.local" dentro de web/.')
  process.exit(1)
}
const sql = neon(url)
const schema = readFileSync(new URL('../server/database/schema.sql', import.meta.url), 'utf8')

// Remove comentários e executa um comando por vez.
const statements = schema
  .split('\n')
  .map((l) => l.replace(/--.*$/, ''))
  .join('\n')
  .split(';')
  .map((s) => s.trim())
  .filter(Boolean)

for (const stmt of statements) {
  await sql.query(stmt)
  console.log('ok:', stmt.split('\n')[0])
}
console.log('Esquema aplicado.')
