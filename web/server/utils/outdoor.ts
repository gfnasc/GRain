// Outdoor: coleta e leitura das observações de fora.
// A coleta roda a cada envio da estação (a cada 10 min): METAR a cada 10 min (o aeroporto
// publica de hora em hora, ou antes quando o tempo muda) e Open-Meteo a cada 30 min.
// Ao detectar o aeroporto (localização nova), busca as últimas 72 h (METAR e Open-Meteo)
// para o histórico.

type OutdoorState = {
  site_key?: string
  station?: MetarStation | null
  metar_try?: number
  om_try?: number
  rain?: RainOutlook | null                // próximas horas da Open-Meteo (método M3)
  taf?: { at: number; groups: TafGroup[]; raw: string } | null
}

const METAR_EVERY = 9 * 60_000
const OM_EVERY = 29 * 60_000

export async function collectOutdoor(deviceId: string): Promise<void> {
  const sql = useSql()
  const [d] = (await sql`select site, outdoor from devices where id = ${deviceId}`) as { site: Site | null; outdoor: OutdoorState | null }[]
  const site = d?.site
  if (!site || site.lat == null || site.lon == null) return
  const st: OutdoorState = { ...(d!.outdoor ?? {}) }
  const now = Date.now()
  const key = `${site.lat},${site.lon}`

  // Aeroporto mais próximo: detectado de novo quando a localização muda.
  let hours = 1
  if (st.site_key !== key) {
    st.station = await nearestMetarStation(site.lat, site.lon)
    st.site_key = key
    st.metar_try = 0
    hours = 72
  }

  const jobs: Promise<void>[] = []
  if (st.station && now - (st.metar_try ?? 0) >= METAR_EVERY) {
    st.metar_try = now
    const icao = st.station.icao
    jobs.push(
      metarHistory(icao, hours).then(async (list) => {
        if (!list.length) return
        const col = <K extends keyof MetarObs>(k: K) => list.map((m) => m[k])
        await sql`
          insert into outdoor_obs (device_id, source, obs_at, station, temp, dew, hum, pres, wind_dir, wind_kmh, gust_kmh, wx, clouds, raw)
          select ${deviceId}, 'metar', o.obs_at, ${icao}, o.temp, o.dew, o.hum, o.pres, o.wind_dir, o.wind_kmh, o.gust_kmh, o.wx, o.clouds, o.raw
          from unnest(
            ${list.map((m) => m.obs_at.toISOString())}::timestamptz[], ${col('temp')}::real[], ${col('dew')}::real[], ${col('hum')}::real[],
            ${col('pres')}::real[], ${col('wind_dir')}::smallint[], ${col('wind_kmh')}::real[], ${col('gust_kmh')}::real[],
            ${col('wx')}::text[], ${list.map((m) => JSON.stringify(m.clouds))}::jsonb[], ${col('raw')}::text[]
          ) as o(obs_at, temp, dew, hum, pres, wind_dir, wind_kmh, gust_kmh, wx, clouds, raw)
          on conflict do nothing`
      }),
    )
  }
  if (now - (st.om_try ?? 0) >= OM_EVERY || hours > 1) {
    st.om_try = now
    jobs.push(
      (hours > 1 ? openMeteoPast(site.lat, site.lon) : openMeteoCurrent(site.lat, site.lon).then((c) => (c ? [c] : []))).then(async (list) => {
        if (!list.length) return
        const col = <K extends keyof CurrentWeather>(k: K) => list.map((c) => c[k])
        await sql`
          insert into outdoor_obs (device_id, source, obs_at, temp, dew, hum, pres, wind_dir, wind_kmh, gust_kmh, code, precip)
          select ${deviceId}, 'open-meteo', o.obs_at, o.temp, o.dew, o.hum, o.pres, o.wind_dir, o.wind_kmh, o.gust_kmh, o.code, o.precip
          from unnest(
            ${list.map((c) => c.obs_at.toISOString())}::timestamptz[], ${col('temp')}::real[], ${col('dew')}::real[], ${col('hum')}::real[],
            ${col('pres')}::real[], ${col('wind_dir')}::smallint[], ${col('wind_kmh')}::real[], ${col('gust_kmh')}::real[], ${col('code')}::smallint[],
            ${col('precip')}::real[]
          ) as o(obs_at, temp, dew, hum, pres, wind_dir, wind_kmh, gust_kmh, code, precip)
          on conflict do nothing`
      }),
    )
  }
  // Previsão das próximas horas (M3) e TAF do aeroporto (M2), também a cada 30 min.
  if (now - (st.rain?.at ?? 0) >= OM_EVERY) {
    jobs.push(openMeteoRain(site.lat, site.lon).then((r) => { if (r) st.rain = r }))
  }
  if (st.station && now - (st.taf?.at ?? 0) >= OM_EVERY) {
    const icao = st.station.icao
    jobs.push(latestTaf(icao).then((t) => { st.taf = t ?? { at: now, groups: [], raw: '' } }))
  }
  const results = await Promise.allSettled(jobs)
  for (const r of results) if (r.status === 'rejected') console.warn('coleta Outdoor falhou:', r.reason)
  await sql`update devices set outdoor = ${JSON.stringify(st)}::jsonb where id = ${deviceId}`
}

export type OutdoorNow = {
  source: 'metar' | 'open-meteo'
  obs_at: string
  temp: number | null
  dew: number | null
  hum: number | null
  feels: number | null
  pres: number | null
  wind_dir: number | null
  wind_kmh: number | null
  gust_kmh: number | null
  condition: string
  raw: string | null
}

const METAR_FRESH = 90 * 60_000

function toNow(r: any): OutdoorNow {
  const temp = r.temp as number | null
  const hum = r.hum as number | null
  return {
    source: r.source,
    obs_at: new Date(r.obs_at).toISOString(),
    temp,
    dew: r.dew,
    hum,
    feels: temp != null && hum != null ? Math.round(heatIndexC(temp, hum) * 10) / 10 : null,
    pres: r.pres,
    wind_dir: r.wind_dir,
    wind_kmh: r.wind_kmh,
    gust_kmh: r.gust_kmh,
    condition: r.source === 'metar' ? describeMetar({ wx: r.wx, clouds: r.clouds ?? [], raw: r.raw ?? '' }) : weatherCodeText(r.code),
    raw: r.raw ?? null,
  }
}

// Observação de fora para mostrar agora: METAR se tiver até 90 min; senão Open-Meteo.
export async function latestOutdoor(deviceId: string) {
  const sql = useSql()
  const [d] = (await sql`select site, outdoor from devices where id = ${deviceId}`) as { site: Site | null; outdoor: OutdoorState | null }[]
  const rows = (await sql`
    select distinct on (source) * from outdoor_obs
    where device_id = ${deviceId} and obs_at > now() - interval '6 hours'
    order by source, obs_at desc`) as any[]
  const metar = rows.find((r) => r.source === 'metar')
  const om = rows.find((r) => r.source === 'open-meteo')
  const metarFresh = metar && Date.now() - new Date(metar.obs_at).getTime() <= METAR_FRESH
  const best = metarFresh ? metar : om ?? metar ?? null
  return {
    located: !!(d?.site && d.site.lat != null && d.site.lon != null),
    station: d?.outdoor?.station ?? null,
    now: best ? toNow(best) : null,
    metar: metar ? toNow(metar) : null,
    model: om ? toNow(om) : null,
  }
}
