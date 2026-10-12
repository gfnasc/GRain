// Login com a senha única do painel (hash em NUXT_PANEL_PASSWORD_HASH).

export default defineEventHandler(async (event) => {
  const { password } = await readBody<{ password?: string }>(event)
  const { panelPasswordHash } = useRuntimeConfig(event)
  if (!panelPasswordHash) throw createError({ statusCode: 500, message: 'senha do painel não configurada' })

  const ok = typeof password === 'string' && password.length > 0 && (await verifyPassword(panelPasswordHash, password))
  if (!ok) {
    // Pequena espera para desestimular tentativas em sequência.
    await new Promise((r) => setTimeout(r, 800))
    throw createError({ statusCode: 401, message: 'Senha incorreta' })
  }

  await setUserSession(event, { user: { name: 'família' }, loggedInAt: Date.now() }, { maxAge: 60 * 60 * 24 * 30 })
  return { ok: true }
})
