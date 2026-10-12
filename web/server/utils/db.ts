import { neon, type NeonQueryFunction } from '@neondatabase/serverless'

// Inicialização preguiçosa: o build não precisa de DATABASE_URL.
let _sql: NeonQueryFunction<false, false> | null = null

export function useSql() {
  if (!_sql) {
    const url = process.env.DATABASE_URL
    if (!url) throw createError({ statusCode: 500, message: 'DATABASE_URL não configurada' })
    _sql = neon(url)
  }
  return _sql
}
