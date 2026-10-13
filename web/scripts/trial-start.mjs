// Marca o início oficial dos testes longos (robustez de 72 h e chuva de 30 dias) no primeiro
// registro de saúde depois do último reinício da estação. Uso, depois de reiniciar a estação
// e esperar o primeiro envio:
//   pnpm trial:start            (usa o primeiro registro depois do último reinício)
//   pnpm trial:start 2026-10-13T20:00:00Z
import { neon } from '@neondatabase/serverless'

const sql = neon(process.env.DATABASE_URL)
const [dev] = await sql`select id from devices order by created_at limit 1`
if (!dev) throw new Error('nenhuma estação cadastrada')
const log = await sql`select ts, fw, uptime_s, boots from status_log where device_id = ${dev.id} order by ts desc limit 300`
if (!log.length) throw new Error('sem registros de saúde')
let at = process.argv[2] ? new Date(process.argv[2]) : null
if (!at) {
  // Do mais novo para trás: para no registro em que o tempo ligado voltou (ou o contador subiu).
  let i = 0
  while (i + 1 < log.length && log[i].uptime_s >= log[i + 1].uptime_s && log[i].boots === log[i + 1].boots) i++
  at = new Date(log[i].ts)
}
const fw = log[0].fw
for (const key of ['robustez', 'chuva']) {
  await sql`insert into trials (device_id, key, started_at, fw) values (${dev.id}, ${key}, ${at.toISOString()}, ${fw})
    on conflict (device_id, key) do update set started_at = excluded.started_at, fw = excluded.fw`
}
console.log(`Início oficial dos testes: ${at.toISOString()} (firmware ${fw}).`)
