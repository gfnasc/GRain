// Configuração da estação (display, calibração e localização): formato, rótulos e validação.
// A configuração é feita só no ESP32 (página local); a estação envia uma cópia junto com
// as médias, o servidor valida com estas regras e o painel só mostra (com a prévia do OLED).

export const PAGE_TYPES = ['temp', 'hum', 'pres', 'clock', 'today', 'fields', 'details', 'network', 'rain'] as const
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
  rain: 'Chuva (próximas 3 h)',
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

export type Location = { lat: number | null; lon: number | null; label: string }
export type RainAlert = { enabled: boolean; threshold: number; hours: 1 | 2 | 3; mode: 'icon' | 'screen' | 'both' }

export type PageMode = 'auto' | 'button' | 'both'
export type LedMode = 'normal' | 'warnings' | 'off'
export const PAGE_MODE_LABELS: Record<PageMode, string> = { auto: 'automática', button: 'pelo botão', both: 'automática e pelo botão' }
export const LED_MODE_LABELS: Record<LedMode, string> = { normal: 'normal', warnings: 'só avisos', off: 'desligado' }

export type StationConfig = {
  display: {
    page_s: number
    contrast: number
    night: { enabled: boolean; from: string; to: string; mode: 'off' | 'dim' }
    pages: PageConfig[]
    page_mode: PageMode
    idle_dim_min: number
  }
  buzzer: { enabled: boolean; clicks: boolean; level: 'high' | 'low' }
  led: { mode: LedMode; quiet: LedMode }
  quiet: { enabled: boolean; from: string; to: string }
  calibration: { temp: number; hum: number; pres: number; pres_auto: boolean; pres_auto_offset: number }
  location: Location
  rain_alert: RainAlert
}

export const MAX_PAGES = 8
export const MAX_FIELDS = 4

const HHMM = /^([01]\d|2[0-3]):[0-5]\d$/

function numberIn(v: unknown, lo: number, hi: number, name: string, integer = false): number {
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi || (integer && !Number.isInteger(v))) {
    throw new Error(`${name}: use ${integer ? 'um número inteiro ' : 'um valor '}entre ${lo} e ${hi}`)
  }
  return integer ? v : Math.round(v * 1e5) / 1e5
}

// Valida e normaliza; lança Error com a mensagem em português.
export function validateStationConfig(input: any): StationConfig {
  const d = input?.display ?? {}
  const n = d.night ?? {}
  const c = input?.calibration ?? {}
  const l = input?.location ?? {}
  const ra = input?.rain_alert ?? {}
  const bz = input?.buzzer ?? {}
  const lg = input?.led ?? {}
  const q = input?.quiet ?? {}
  const ledMode = (v: unknown, def: LedMode): LedMode => (v === 'normal' || v === 'warnings' || v === 'off' ? v : def)

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
  const hasLoc = l.lat != null && l.lon != null
  if (hasLoc !== (l.lat != null || l.lon != null)) throw new Error('localização: latitude e longitude juntas')
  if (l.label != null && (typeof l.label !== 'string' || l.label.length > 40)) throw new Error('localização: nome com até 40 caracteres')

  return {
    display: {
      page_s: numberIn(d.page_s, 2, 60, 'tempo por página', true),
      contrast: numberIn(d.contrast, 0, 255, 'brilho', true),
      night: { enabled: n.enabled, from: n.from, to: n.to, mode: n.mode },
      pages,
      page_mode: d.page_mode === 'button' || d.page_mode === 'both' ? d.page_mode : 'auto',
      idle_dim_min: numberIn(d.idle_dim_min ?? 10, 0, 120, 'escurecer sem toque', true),
    },
    buzzer: { enabled: bz.enabled !== false, clicks: bz.clicks !== false, level: bz.level === 'low' ? 'low' : 'high' },
    led: { mode: ledMode(lg.mode, 'normal'), quiet: ledMode(lg.quiet, 'warnings') },
    quiet: {
      enabled: q.enabled !== false,
      from: HHMM.test(q.from) ? q.from : '22:00',
      to: HHMM.test(q.to) ? q.to : '07:00',
    },
    calibration: {
      temp: numberIn(c.temp, -5, 5, 'ajuste de temperatura'),
      hum: numberIn(c.hum, -15, 15, 'ajuste de umidade'),
      pres: numberIn(c.pres, -15, 15, 'ajuste de pressão'),
      pres_auto: c.pres_auto !== false,
      pres_auto_offset: numberIn(c.pres_auto_offset ?? 0, -15, 15, 'ajuste automático de pressão'),
    },
    rain_alert: {
      enabled: ra.enabled !== false,
      threshold: numberIn(ra.threshold ?? 60, 10, 100, 'limite do alerta de chuva', true),
      hours: numberIn(ra.hours ?? 2, 1, 3, 'janela do alerta de chuva', true) as 1 | 2 | 3,
      mode: ra.mode === 'icon' || ra.mode === 'screen' ? ra.mode : 'both',
    },
    location: {
      lat: hasLoc ? Math.round(numberIn(l.lat, -90, 90, 'latitude') * 1e5) / 1e5 : null,
      lon: hasLoc ? Math.round(numberIn(l.lon, -180, 180, 'longitude') * 1e5) / 1e5 : null,
      label: l.label ?? '',
    },
  }
}
