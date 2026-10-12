# GWeather — painel (Nuxt na Vercel)

Painel web e API de nuvem da estação. Recebe as médias de 10 min enviadas pelo ESP32, guarda no Postgres (Neon) e mostra os dados com login.

## Stack
- Nuxt 4 + Nitro (rotas de servidor em `server/api`)
- Postgres (Neon, pelo Marketplace da Vercel) com `@neondatabase/serverless`
- Login com senha única via `nuxt-auth-utils` (sessão em cookie criptografado, 30 dias)
- Gráficos com [uPlot](https://github.com/leeoniya/uPlot)
- Fuso dos horários e dos dias: `NUXT_PUBLIC_TZ` (ex.: `America/Sao_Paulo`; padrão `UTC`)

## Rotas

| Rota | Autenticação | Descrição |
|---|---|---|
| `POST /api/v1/ingest` | `Authorization: Bearer <token da estação>` | Recebe lote de médias; horários repetidos são ignorados |
| `GET /api/v1/latest` | Sessão do painel | Leitura mais recente, últimas 12 médias, mín./máx. de 24 h e estado da estação |
| `GET /api/v1/readings` | Sessão do painel | Série agregada para os gráficos (`?range=24h\|7d\|30d` ou `?from=&to=`) |
| `GET /api/v1/daily` | Sessão do painel | Resumo por dia |
| `GET /api/v1/readings.csv` | Sessão do painel | Exportação CSV (`&excel=1` para Excel em português) |
| `POST /api/auth/login` | — | Login com a senha do painel |

Contrato completo: [../docs/api-v1.md](../docs/api-v1.md).

## Configurar do zero

Requisitos: Node 20+, pnpm e [Vercel CLI](https://vercel.com/docs/cli) logado.

```bash
cd web
pnpm install
vercel link                      # cria/vincula o projeto na Vercel
vercel integration add neon      # cria o banco e as variáveis DATABASE_URL etc.
vercel env pull .env.local
pnpm db:migrate                  # cria as tabelas
```

Senha do painel e chave da sessão (variáveis na Vercel):

```bash
pnpm panel:password              # gera senha aleatória em .panel-password.txt e imprime o hash
vercel env add NUXT_PANEL_PASSWORD_HASH production   # cole o hash
# NUXT_SESSION_PASSWORD: texto aleatório com 32+ caracteres
vercel env add NUXT_SESSION_PASSWORD production
```

Cadastrar a estação (o `device_id` aparece em `config` no console serial do ESP32):

```bash
pnpm device:token gweather-xxxxxx   # token em .device-token-gweather-xxxxxx.txt
```

No console serial do ESP32:

```
nuvem url https://SEU-PROJETO.vercel.app
nuvem token <conteúdo do arquivo>
```

`.env.local`, `.panel-password.txt` e `.device-token-*.txt` ficam fora do git.

## Desenvolvimento

```bash
pnpm dev        # http://localhost:3000 (usa .env.local)
pnpm build
```
