// Configuração do display e da calibração: formato, valores padrão e validação.
// Compartilhado entre o servidor (validação) e o painel (editor e prévia).
// O ESP32 recebe exatamente este JSON e grava em /config.json.

export const PAGE_TYPES = ['temp', 'hum', 'pres', 'clock', 'today', 'fields', 'details', 'network'] as const
export type PageType = (typeof PAGE_TYPES)[number]

export const FIELD_TYPES = ['temp', 'hum', 'pres', 'dew', 'heat_index', 'temp_bmp', 'pres_station'] as const
export type FieldType = (typeof FIELD_TYPES)[number]

export const PAGE_LABELS: Record<PageType, string> = {
  temp: 'Temperatura',
  hum: 'Umidade',
  pres: 'Pressão',
  clock: 'Relógio',
  today: 'Hoje (mín. e máx.)',
  fields: 'Resumo (campos à escolha)',
  details: 'Detalhes',
  network: 'Rede',
}

export const FIELD_LABELS: Record<FieldType, string> = {
  temp: 'Temperatura',
  hum: 'Umidade',
  pres: 'Pressão (nível do mar)',
  dew: 'Ponto de orvalho',
  heat_index: 'Sensação térmica',
  temp_bmp: 'Temperatura do BMP280',
  pres_station: 'Pressão no local',
}

export type PageConfig = { type: PageType; fields?: FieldType[] }

export type StationConfig = {
  display: {
    page_s: number
    contrast: number
    night: { enabled: boolean; from: string; to: string; mode: 'off' | 'dim' }
    pages: PageConfig[]
  }
  calibration: { temp: number; hum: number; pres: number }
}

export const MAX_PAGES = 8
export const MAX_FIELDS = 4

export const DEFAULT_CONFIG: StationConfig = {
  display: {
    page_s: 5,
    contrast: 128,
    night: { enabled: false, from: '23:00', to: '06:00', mode: 'dim' },
    pages: [{ type: 'temp' }, { type: 'pres' }, { type: 'details' }, { type: 'network' }],
  },
  calibration: { temp: 0, hum: 0, pres: 0 },
}

const HHMM = /^([01]\d|2[0-3]):[0-5]\d$/

function numberIn(v: unknown, lo: number, hi: number, name: string, integer = false): number {
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi || (integer && !Number.isInteger(v))) {
    throw new Error(`${name}: use ${integer ? 'um número inteiro ' : 'um valor '}entre ${lo} e ${hi}`)
  }
  return integer ? v : Math.round(v * 10) / 10
}

// Valida e normaliza; lança Error com a mensagem em português.
export function validateStationConfig(input: any): StationConfig {
  const d = input?.display ?? {}
  const n = d.night ?? {}
  const c = input?.calibration ?? {}

  if (!Array.isArray(d.pages) || d.pages.length < 1 || d.pages.length > MAX_PAGES) {
    throw new Error(`páginas: escolha de 1 a ${MAX_PAGES}`)
  }
  const pages: PageConfig[] = d.pages.map((p: any, i: number) => {
    if (!PAGE_TYPES.includes(p?.type)) throw new Error(`página ${i + 1}: tipo desconhecido`)
    if (p.type !== 'fields') return { type: p.type }
    const fields = Array.isArray(p.fields) ? p.fields : []
    if (fields.length < 1 || fields.length > MAX_FIELDS) throw new Error(`página ${i + 1}: escolha de 1 a ${MAX_FIELDS} campos`)
    for (const f of fields) if (!FIELD_TYPES.includes(f)) throw new Error(`página ${i + 1}: campo desconhecido`)
    return { type: 'fields', fields }
  })

  if (typeof n.enabled !== 'boolean') throw new Error('modo noturno: informe ligado ou desligado')
  if (!HHMM.test(n.from) || !HHMM.test(n.to)) throw new Error('modo noturno: use horários no formato HH:MM')
  if (n.from === n.to) throw new Error('modo noturno: início e fim não podem ser iguais')
  if (n.mode !== 'off' && n.mode !== 'dim') throw new Error('modo noturno: escolha apagar ou escurecer')

  return {
    display: {
      page_s: numberIn(d.page_s, 2, 60, 'tempo por página', true),
      contrast: numberIn(d.contrast, 0, 255, 'brilho', true),
      night: { enabled: n.enabled, from: n.from, to: n.to, mode: n.mode },
      pages,
    },
    calibration: {
      temp: numberIn(c.temp, -5, 5, 'ajuste de temperatura'),
      hum: numberIn(c.hum, -15, 15, 'ajuste de umidade'),
      pres: numberIn(c.pres, -15, 15, 'ajuste de pressão'),
    },
  }
}
