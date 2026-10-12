// Previsão e cálculos para as mensagens.

// Sensação térmica (índice de calor NOAA), °C. Mesma fórmula do firmware.
export function heatIndexC(tempC: number, hum: number): number {
  const t = (tempC * 9) / 5 + 32
  let hi = 0.5 * (t + 61 + (t - 68) * 1.2 + hum * 0.094)
  if ((hi + t) / 2 >= 80) {
    hi = -42.379 + 2.04901523 * t + 10.14333127 * hum - 0.22475541 * t * hum - 0.00683783 * t * t -
      0.05481717 * hum * hum + 0.00122874 * t * t * hum + 0.00085282 * t * hum * hum - 0.00000199 * t * t * hum * hum
    if (hum < 13 && t >= 80 && t <= 112) hi -= ((13 - hum) / 4) * Math.sqrt((17 - Math.abs(t - 95)) / 17)
    else if (hum > 85 && t >= 80 && t <= 87) hi += ((hum - 85) / 10) * ((87 - t) / 5)
  }
  return ((hi - 32) * 5) / 9
}

export type Trend = 'subindo' | 'estável' | 'caindo'

// Tendência em 3 h (limiar de 1,6 hPa, usado em previsão por barômetro).
export function trendOf(delta3h: number | null): Trend | null {
  if (delta3h === null) return null
  if (delta3h >= 1.6) return 'subindo'
  if (delta3h <= -1.6) return 'caindo'
  return 'estável'
}

// Previsão de Zambretti: pressão ao nível do mar + tendência de 3 h.
// Feita para climas temperados; aqui serve como indicação de tendência local.
const ZAMBRETTI = [
  'tempo firme', 'tempo bom', 'bom, ficando instável', 'razoável, pancadas mais tarde', 'pancadas, cada vez mais instável',
  'instável, chuva mais tarde', 'chuva às vezes, piorando', 'chuva às vezes, muito instável', 'muito instável, chuva',
  'tempo firme', 'tempo bom', 'bom, possíveis pancadas', 'razoável, pancadas prováveis', 'pancadas com aberturas de sol',
  'variável, alguma chuva', 'instável, chuva às vezes', 'chuva frequente', 'muito instável, chuva', 'tempestuoso, muita chuva',
  'tempo firme', 'tempo bom', 'melhorando', 'razoável, melhorando', 'razoável, pancadas no início',
  'pancadas no início, melhorando', 'variável, melhorando', 'instável, abrindo mais tarde', 'instável, provável melhora',
  'instável, curtos períodos de sol', 'muito instável, melhor às vezes', 'tempestuoso, possível melhora', 'tempestuoso, muita chuva',
]

export function zambretti(pres: number, trend: Trend): string {
  let z: number
  if (trend === 'caindo') z = Math.round(127 - 0.12 * pres)
  else if (trend === 'subindo') z = Math.round(185 - 0.16 * pres)
  else z = Math.round(144 - 0.13 * pres)
  const [lo, hi] = trend === 'caindo' ? [1, 9] : trend === 'subindo' ? [20, 32] : [10, 19]
  z = Math.min(hi, Math.max(lo, z))
  return ZAMBRETTI[z - 1]!
}

// Códigos WMO usados pela Open-Meteo.
export function weatherCodeText(code: number | null | undefined): string {
  if (code == null) return ''
  if (code === 0) return 'céu limpo'
  if (code === 1) return 'predomínio de sol'
  if (code === 2) return 'parcialmente nublado'
  if (code === 3) return 'nublado'
  if (code === 45 || code === 48) return 'neblina'
  if (code >= 51 && code <= 57) return 'garoa'
  if (code === 61) return 'chuva fraca'
  if (code === 63) return 'chuva moderada'
  if (code === 65) return 'chuva forte'
  if (code === 66 || code === 67) return 'chuva congelante'
  if (code >= 71 && code <= 77) return 'neve'
  if (code === 80) return 'pancadas fracas'
  if (code === 81) return 'pancadas de chuva'
  if (code === 82) return 'pancadas fortes'
  if (code === 95) return 'trovoadas'
  if (code === 96 || code === 99) return 'trovoadas com granizo'
  return ''
}

export type DayForecast = { tmax: number; tmin: number; rain: number | null; code: number | null; text: string }

// Previsão diária da Open-Meteo (sem chave). Guardada em memória por 30 min.
const cache = new Map<string, { at: number; days: DayForecast[] }>()

export async function openMeteo(lat: number, lon: number, tz: string): Promise<DayForecast[] | null> {
  const key = `${lat},${lon},${tz}`
  const hit = cache.get(key)
  if (hit && Date.now() - hit.at < 30 * 60_000) return hit.days
  const url = new URL('https://api.open-meteo.com/v1/forecast')
  url.search = new URLSearchParams({
    latitude: String(lat),
    longitude: String(lon),
    daily: 'temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code',
    timezone: tz,
    forecast_days: '2',
  }).toString()
  try {
    const r = await fetch(url, { signal: AbortSignal.timeout(6000) })
    if (!r.ok) return null
    const j: any = await r.json()
    const d = j.daily
    const days: DayForecast[] = (d.time as string[]).map((_, i) => ({
      tmax: d.temperature_2m_max[i],
      tmin: d.temperature_2m_min[i],
      rain: d.precipitation_probability_max?.[i] ?? null,
      code: d.weather_code?.[i] ?? null,
      text: weatherCodeText(d.weather_code?.[i]),
    }))
    cache.set(key, { at: Date.now(), days })
    return days
  } catch {
    return null
  }
}
