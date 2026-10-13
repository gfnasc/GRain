# GRain — painel (Nuxt na Vercel)

Painel web e API de nuvem da estação. Recebe as médias de 10 min enviadas pelo ESP32, guarda no Postgres (Neon) e mostra os dados com login. Só visualização: a configuração é feita na página local do ESP32, que manda uma cópia junto com as médias. O local da estação em cada período fica na tabela `placements`: temperatura e umidade medidas na sacada aparecem marcadas e ficam fora dos extremos.

## Stack
- Nuxt 4 + Nitro (rotas de servidor em `server/api`)
- Postgres (Neon, pelo Marketplace da Vercel) com `@neondatabase/serverless`
- Login com senha única via `nuxt-auth-utils` (sessão em cookie criptografado, 30 dias)
- Gráficos com [uPlot](https://github.com/leeoniya/uPlot)
- Fuso dos horários e dos dias: `NUXT_PUBLIC_TZ` (ex.: `America/Sao_Paulo`; padrão `UTC`)
- Dados de fora: METAR (aviationweather.gov) e Open-Meteo, sem chave; coleta disparada por `POST /api/v1/ingest` e executada depois da resposta (`waitUntil` de `@vercel/functions`)

## Rotas

| Rota | Autenticação | Descrição |
|---|---|---|
| `POST /api/v1/ingest` | `Authorization: Bearer <token da estação>` | Recebe lote de médias, estado e cópia da configuração; horários repetidos são ignorados |
| `GET /api/v1/latest` | Sessão do painel | Leitura mais recente, últimas 12 médias, mín./máx. de 24 h (sem a sacada), local e estado da estação |
| `GET /api/v1/readings` | Sessão do painel | Série agregada para os gráficos (`?range=24h\|7d\|30d` ou `?from=&to=`) e períodos de cada local |
| `GET /api/v1/daily` | Sessão do painel | Resumo por dia |
| `GET /api/v1/readings.csv` | Sessão do painel | Exportação CSV com a coluna `local` (`&excel=1` para Excel em português) |
| `GET /api/v1/indoor` | Sessão do painel | Conforto, ar seco, mofo, janela e atraso térmico do ambiente da estação |
| `GET /api/v1/sync` | Sessão do painel | Horários das últimas e próximas atualizações (contadores) |
| `GET /api/v1/station` | Sessão do painel | Estado da estação e cópia da configuração (somente leitura) |
| `GET /api/v1/outdoor` | Sessão do painel | Condições de fora: METAR do aeroporto mais próximo ou Open-Meteo |
| `GET /api/v1/forecast` | Sessão do painel | Previsão de hoje e amanhã e tendência da pressão |
| `GET /api/v1/rain`, `/rain/history`, `/rain/score` | Sessão do painel | Chuva em 1, 2 e 3 h (três métodos), histórico das previsões e placar |
| `POST /api/v1/rain/report` | Sessão do painel | Aviso "Está chovendo aqui" |
| `POST /api/v1/device/rain-report` | Token da estação | O mesmo aviso, vindo da página local |
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
pnpm device:token grain-xxxxxx   # token em .device-token-grain-xxxxxx.txt
```

No console serial do ESP32:

```
nuvem url https://SEU-PROJETO.vercel.app
nuvem token <conteúdo do arquivo>
```

Registrar onde a estação está (opcional; sem registro, nada é marcado):

```bash
pnpm placement escritorio            # a partir de agora; ou passe um horário ISO
pnpm placement                       # lista os períodos
```

`.env.local`, `.panel-password.txt` e `.device-token-*.txt` ficam fora do git.

## Desenvolvimento

```bash
pnpm dev        # http://localhost:3000 (usa .env.local)
pnpm build
```
