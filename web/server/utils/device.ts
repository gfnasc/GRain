// O painel mostra uma estação: a primeira cadastrada.
export async function defaultDeviceId(): Promise<string | null> {
  const sql = useSql()
  const rows = (await sql`select id from devices order by created_at limit 1`) as { id: string }[]
  return rows[0]?.id ?? null
}
