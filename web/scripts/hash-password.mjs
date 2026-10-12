// Gera o hash (scrypt, mesmo formato do nuxt-auth-utils) da senha do painel.
// Uso:
//   pnpm panel:password            gera uma senha aleatória
//   pnpm panel:password "minha senha"
// Grava a senha em .panel-password.txt (fora do git) e imprime só o hash,
// para ser cadastrado na Vercel como NUXT_PANEL_PASSWORD_HASH.
import { writeFileSync } from 'node:fs'
import { randomBytes } from 'node:crypto'
import { Hash } from '@adonisjs/hash'
import { Scrypt } from '@adonisjs/hash/drivers/scrypt'

const password = process.argv[2] || randomBytes(12).toString('base64url')
if (password.length < 10) {
  console.error('Use pelo menos 10 caracteres.')
  process.exit(1)
}
writeFileSync(new URL('../.panel-password.txt', import.meta.url), password + '\n', { mode: 0o600 })
console.log(await new Hash(new Scrypt({})).make(password))
