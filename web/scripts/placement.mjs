// Registra onde a estação está (sacada ou escritório) a partir de um horário.
// Uso: pnpm placement <sacada|escritorio> [quando] [device_id]
//   quando: data/hora ISO (ex.: 2026-10-13T08:10-03:00); padrão: agora.
//   Sem nenhum registro anterior, o primeiro período começa na leitura mais antiga.
//   pnpm placement          → lista os períodos
import { neon } from '@neondatabase/serverless'

const PLACES = ['sacada', 'escritorio']
const [place, when, idArg] = process.argv.slice(2)
const url = process.env.DATABASE_URL
if (!url) {
  console.error('DATABASE_URL não definida. Rode "vercel env pull .env.local" dentro de web/.')
  process.exit(1)
}
const sql = neon(url)

const [dev] = idArg ? [{ id: idArg }] : await sql`select id from devices order by created_at limit 1`
if (!dev) {
  console.error('Nenhuma estação cadastrada.')
  process.exit(1)
}

async function list() {
  const rows = await sql`select place, started_at, ended_at from placements where device_id = ${dev.id} order by started_at`
  if (!rows.length) console.log('Nenhum período registrado.')
  for (const r of rows) console.log(`${r.place.padEnd(11)} ${new Date(r.started_at).toISOString()} → ${r.ended_at ? new Date(r.ended_at).toISOString() : 'agora'}`)
}

if (!place) {
  await list()
  process.exit(0)
}
if (!PLACES.includes(place)) {
  console.error(`Local inválido: use ${PLACES.join(' ou ')}.`)
  process.exit(1)
}
const at = when ? new Date(when) : new Date()
if (Number.isNaN(at.getTime())) {
  console.error('Horário inválido. Exemplo: 2026-10-13T08:10-03:00')
  process.exit(1)
}

const [current] = await sql`select id, place, started_at from placements where device_id = ${dev.id} and ended_at is null`
if (current?.place === place) {
  console.log(`A estação já está em "${place}" desde ${new Date(current.started_at).toISOString()}.`)
  process.exit(0)
}
if (current && at <= new Date(current.started_at)) {
  console.error(`O horário precisa ser depois do início do período atual (${new Date(current.started_at).toISOString()}).`)
  process.exit(1)
}

if (current) {
  await sql.transaction([
    sql`update placements set ended_at = ${at.toISOString()} where id = ${current.id}`,
    sql`insert into placements (device_id, place, started_at) values (${dev.id}, ${place}, ${at.toISOString()})`,
  ])
} else {
  const [{ first }] = await sql`select min(ts) as first from readings where device_id = ${dev.id}`
  const start = when ? at : first ? new Date(first) : at
  await sql`insert into placements (device_id, place, started_at) values (${dev.id}, ${place}, ${start.toISOString()})`
}
await list()
