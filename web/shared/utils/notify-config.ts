// Configuração das notificações: agenda, alertas e local da previsão.
// Compartilhado entre o servidor (validação) e o painel (editor).

export const MESSAGE_TYPES = ['forecast', 'current', 'trend', 'summary'] as const
export type MessageType = (typeof MESSAGE_TYPES)[number]

export const MESSAGE_LABELS: Record<MessageType, string> = {
  forecast: 'Previsão do dia',
  current: 'Condições atuais',
  trend: 'Tendência (pressão)',
  summary: 'Resumo do dia',
}

export type Schedule = {
  id: string          // estável: identifica o envio no registro (não duplica)
  time: string        // HH:MM no fuso do painel
  type: MessageType
  days: number[]      // 0 = domingo ... 6 = sábado
  enabled: boolean
}

export type NotifyConfig = {
  enabled: boolean
  location: { lat: number | null; lon: number | null; label: string }
  schedules: Schedule[]
  alerts: {
    enabled: boolean
    temp_max: number | null     // °C; null = desligado
    temp_min: number | null
    pres_drop_3h: number | null // hPa de queda em 3 h
    min_gap_h: number           // intervalo mínimo entre alertas do mesmo tipo
  }
}

export const MAX_SCHEDULES = 8
const ALL_DAYS = [0, 1, 2, 3, 4, 5, 6]

export const DEFAULT_NOTIFY: NotifyConfig = {
  enabled: true,
  location: { lat: null, lon: null, label: '' },
  schedules: [
    { id: 'manha', time: '07:00', type: 'forecast', days: ALL_DAYS, enabled: true },
    { id: 'tarde', time: '13:00', type: 'current', days: ALL_DAYS, enabled: true },
    { id: 'noite', time: '19:00', type: 'trend', days: ALL_DAYS, enabled: true },
    { id: 'resumo', time: '22:00', type: 'summary', days: ALL_DAYS, enabled: false },
  ],
  alerts: { enabled: true, temp_max: 35, temp_min: 8, pres_drop_3h: 4, min_gap_h: 3 },
}

const HHMM = /^([01]\d|2[0-3]):[0-5]\d$/
const ID = /^[a-z0-9-]{1,24}$/

function optNumber(v: unknown, lo: number, hi: number, name: string): number | null {
  if (v === null || v === undefined || v === '') return null
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi) throw new Error(`${name}: use um valor entre ${lo} e ${hi}`)
  return Math.round(v * 10) / 10
}

export function validateNotifyConfig(input: any): NotifyConfig {
  if (typeof input?.enabled !== 'boolean') throw new Error('notificações: informe ligado ou desligado')
  const loc = input.location ?? {}
  const lat = optNumber(loc.lat, -90, 90, 'latitude')
  const lon = optNumber(loc.lon, -180, 180, 'longitude')
  if ((lat === null) !== (lon === null)) throw new Error('local: informe latitude e longitude juntas')
  const label = typeof loc.label === 'string' ? loc.label.slice(0, 40) : ''

  const sch = input.schedules
  if (!Array.isArray(sch) || sch.length > MAX_SCHEDULES) throw new Error(`agenda: até ${MAX_SCHEDULES} horários`)
  const ids = new Set<string>()
  const schedules: Schedule[] = sch.map((s: any, i: number) => {
    const n = `horário ${i + 1}`
    if (!ID.test(s?.id)) throw new Error(`${n}: identificador inválido`)
    if (ids.has(s.id)) throw new Error(`${n}: identificador repetido`)
    ids.add(s.id)
    if (!HHMM.test(s.time)) throw new Error(`${n}: use HH:MM`)
    if (!MESSAGE_TYPES.includes(s.type)) throw new Error(`${n}: tipo de mensagem desconhecido`)
    const days = Array.isArray(s.days) ? [...new Set(s.days as number[])].filter((d) => Number.isInteger(d) && d >= 0 && d <= 6).sort() : []
    if (!days.length) throw new Error(`${n}: escolha ao menos um dia`)
    if (typeof s.enabled !== 'boolean') throw new Error(`${n}: informe ligado ou desligado`)
    return { id: s.id, time: s.time, type: s.type, days, enabled: s.enabled }
  })

  const a = input.alerts ?? {}
  if (typeof a.enabled !== 'boolean') throw new Error('alertas: informe ligado ou desligado')
  const gap = optNumber(a.min_gap_h, 1, 24, 'intervalo entre alertas')
  return {
    enabled: input.enabled,
    location: { lat, lon, label },
    schedules,
    alerts: {
      enabled: a.enabled,
      temp_max: optNumber(a.temp_max, -20, 60, 'alerta de calor'),
      temp_min: optNumber(a.temp_min, -30, 40, 'alerta de frio'),
      pres_drop_3h: optNumber(a.pres_drop_3h, 0.5, 20, 'alerta de queda de pressão'),
      min_gap_h: gap ?? 3,
    },
  }
}

// Telefone no formato internacional (+ seguido de 10 a 15 dígitos).
export function normalizePhone(v: string): string | null {
  const digits = (v ?? '').replace(/[^\d+]/g, '')
  const p = digits.startsWith('+') ? digits : `+${digits}`
  return /^\+\d{10,15}$/.test(p) ? p : null
}
