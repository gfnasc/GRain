// METAR do aeroporto mais próximo (aviationweather.gov, sem chave) e tradução para português.

const API = 'https://aviationweather.gov/api/data/metar'
const UA = 'GWethr/1.0 (estacao meteorologica pessoal; github.com/gfnasc/GWethr)'

export type MetarStation = { icao: string; name: string; lat: number; lon: number; dist_km: number }
export type MetarObs = {
  icao: string
  obs_at: Date
  temp: number | null
  dew: number | null
  hum: number | null
  pres: number | null
  wind_dir: number | null
  wind_kmh: number | null
  gust_kmh: number | null
  wx: string | null
  clouds: { cover: string; base: number | null }[]
  raw: string
}

const KT = 1.852

export function distanceKm(lat1: number, lon1: number, lat2: number, lon2: number): number {
  const r = (d: number) => (d * Math.PI) / 180
  const a = Math.sin(r(lat2 - lat1) / 2) ** 2 + Math.cos(r(lat1)) * Math.cos(r(lat2)) * Math.sin(r(lon2 - lon1) / 2) ** 2
  return 6371 * 2 * Math.asin(Math.sqrt(a))
}

// Umidade relativa a partir da temperatura e do ponto de orvalho (Magnus).
export function relHumidity(t: number, td: number): number {
  const f = (x: number) => Math.exp((17.625 * x) / (243.04 + x))
  return Math.min(100, Math.round((100 * f(td)) / f(t)))
}

async function query(params: Record<string, string>): Promise<any[]> {
  const url = `${API}?${new URLSearchParams({ format: 'json', ...params })}`
  const r = await fetch(url, { headers: { 'User-Agent': UA }, signal: AbortSignal.timeout(8000) })
  if (r.status === 204) return []
  if (!r.ok) throw new Error(`METAR: HTTP ${r.status}`)
  const j = await r.json()
  return Array.isArray(j) ? j : []
}

function parse(m: any): MetarObs {
  const temp = typeof m.temp === 'number' ? m.temp : null
  const dew = typeof m.dewp === 'number' ? m.dewp : null
  return {
    icao: m.icaoId,
    obs_at: new Date(m.obsTime * 1000),
    temp,
    dew,
    hum: temp != null && dew != null ? relHumidity(temp, dew) : null,
    pres: typeof m.altim === 'number' ? m.altim : null,
    wind_dir: typeof m.wdir === 'number' ? m.wdir : null,
    wind_kmh: typeof m.wspd === 'number' ? Math.round(m.wspd * KT) : null,
    gust_kmh: typeof m.wgst === 'number' ? Math.round(m.wgst * KT) : null,
    wx: m.wxString || null,
    clouds: Array.isArray(m.clouds) ? m.clouds.map((c: any) => ({ cover: c.cover, base: c.base ?? null })) : [],
    raw: m.rawOb ?? '',
  }
}

// Aeroporto com METAR mais próximo da localização (procura num raio crescente).
export async function nearestMetarStation(lat: number, lon: number): Promise<MetarStation | null> {
  for (const d of [1, 2.5]) {
    const list = await query({ bbox: `${lat - d},${lon - d},${lat + d},${lon + d}` })
    const near = list
      .filter((m) => typeof m.lat === 'number' && typeof m.lon === 'number' && m.icaoId)
      .map((m) => ({ icao: m.icaoId as string, name: (m.name as string) ?? m.icaoId, lat: m.lat, lon: m.lon, dist_km: Math.round(distanceKm(lat, lon, m.lat, m.lon)) }))
      .sort((a, b) => a.dist_km - b.dist_km)
    if (near.length) return near[0]!
  }
  return null
}

// Observações do aeroporto nas últimas horas (1 = só a mais recente).
export async function metarHistory(icao: string, hours = 1): Promise<MetarObs[]> {
  const list = await query({ ids: icao, hours: String(hours) })
  return list.filter((m) => m?.obsTime).map(parse)
}

// --- tradução ---

const PHENOMENA: [string, string][] = [
  ['TSRA', 'trovoada com chuva'], ['SHRA', 'pancadas de chuva'], ['TS', 'trovoada'], ['RA', 'chuva'], ['DZ', 'garoa'],
  ['GR', 'granizo'], ['GS', 'granizo pequeno'], ['FG', 'nevoeiro'], ['BR', 'névoa'], ['HZ', 'névoa seca'], ['FU', 'fumaça'], ['SH', 'pancadas'],
]

function phenomenon(token: string): string | null {
  let t = token
  const near = t.startsWith('VC')
  if (near) t = t.slice(2)
  const intensity = t[0] === '-' ? ' fraca' : t[0] === '+' ? ' forte' : ''
  t = t.replace(/^[-+]/, '')
  const hit = PHENOMENA.find(([k]) => t.includes(k))
  if (!hit) return null
  return `${hit[1]}${intensity}${near ? ' nas proximidades' : ''}`
}

const COVER: Record<string, [number, string]> = {
  FEW: [1, 'poucas nuvens'], SCT: [2, 'parcialmente nublado'], BKN: [3, 'nublado'], OVC: [4, 'encoberto'], VV: [4, 'encoberto'],
}

// Condição em português: tempo presente, ou cobertura do céu, mais aviso de nuvens de trovoada.
export function describeMetar(o: Pick<MetarObs, 'wx' | 'clouds' | 'raw'>): string {
  const parts: string[] = []
  const wx = (o.wx ?? '').split(/\s+/).filter(Boolean).map(phenomenon).filter(Boolean) as string[]
  if (wx.length) parts.push(wx.join(', '))
  else if (/\bCAVOK\b/.test(o.raw)) parts.push('céu limpo')
  else {
    const top = o.clouds.map((c) => COVER[c.cover]).filter(Boolean).sort((a, b) => b![0] - a![0])[0]
    parts.push(top ? top[1] : 'céu limpo')
  }
  if (/\b(FEW|SCT|BKN|OVC)\d{3}(CB|TCU)\b/.test(o.raw)) parts.push('nuvens de trovoada por perto')
  return parts.join(' · ')
}

export type TafGroup = { from: number; to: number; change: string | null; prob: number | null; wx: string | null; cb: boolean }

// TAF (previsão do aeroporto para 24–30 h). Nem todo aeroporto tem; sem TAF devolve null.
export async function latestTaf(icao: string): Promise<{ at: number; groups: TafGroup[]; raw: string } | null> {
  const r = await fetch(`https://aviationweather.gov/api/data/taf?${new URLSearchParams({ ids: icao, format: 'json' })}`, {
    headers: { 'User-Agent': UA },
    signal: AbortSignal.timeout(8000),
  })
  if (r.status === 204) return null
  if (!r.ok) throw new Error(`TAF: HTTP ${r.status}`)
  const list = await r.json()
  const t = Array.isArray(list) ? list[0] : null
  if (!t?.fcsts) return null
  return {
    at: Date.now(),
    raw: t.rawTAF ?? '',
    groups: t.fcsts.map((f: any) => ({
      from: f.timeFrom,
      to: f.timeTo,
      change: f.fcstChange ?? null,
      prob: typeof f.probability === 'number' ? f.probability : null,
      wx: f.wxString ?? null,
      cb: Array.isArray(f.clouds) && f.clouds.some((c: any) => c.type === 'CB' || c.type === 'TCU'),
    })),
  }
}

// Chuva de verdade no tempo presente do METAR (fora "nas proximidades"): RA, DZ, SHRA, TSRA, GR...
export const METAR_RAIN_SQL = String.raw`(^|\s)[-+]?(TS|SH)?(RA|DZ|GR|GS|UP)`
export function metarIsRaining(wx: string | null): boolean {
  return !!wx && new RegExp(METAR_RAIN_SQL).test(wx)
}
