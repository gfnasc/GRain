// Previsão consolidada para a aba Agora: Open-Meteo (hoje e amanhã) + tendência da
// pressão medida pela estação (que vale dentro e fora de casa).

export type ForecastSummary = {
  location: { configured: boolean; label: string }
  today: DayForecast | null
  tomorrow: DayForecast | null
  pressure: { value: number | null; delta_3h: number | null; trend: Trend | null; outlook: string | null }
}

export async function buildForecast(deviceId: string, loc: Site, tz: string): Promise<ForecastSummary> {
  const configured = loc.lat != null && loc.lon != null
  const days = configured ? await openMeteo(loc.lat!, loc.lon!, tz) : null
  const latest = await latestRow(deviceId)
  const fresh = latest && Date.now() - latest.ts.getTime() < 30 * 60_000
  const delta = fresh ? await pressureDelta3h(deviceId, latest) : null
  const trend = trendOf(delta)
  const pres = fresh ? latest!.pres_slp : null
  return {
    location: { configured, label: loc.label },
    today: days?.[0] ?? null,
    tomorrow: days?.[1] ?? null,
    pressure: {
      value: pres,
      delta_3h: delta == null ? null : Math.round(delta * 10) / 10,
      trend,
      outlook: trend && pres != null ? zambretti(pres, trend) : null,
    },
  }
}
