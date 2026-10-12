<script setup lang="ts">
// Prévia do OLED 128×64. Desenha a página com o mesmo layout do firmware
// (firmware/src/display.cpp) e converte para 1 bit (pixel aceso ou apagado),
// então a aparência é próxima da tela real. Valores vêm da última média.
type Reading = { temp: number | null; hum: number | null; pres_slp: number | null; dew: number | null }
const props = defineProps<{
  page: PageConfig
  index: number
  count: number
  reading: Reading | null
  today: { temp_min: number | null; temp_max: number | null; hum_min: number | null; hum_max: number | null } | null
  dim?: boolean
}>()

const tz = useRuntimeConfig().public.tz as string
const canvas = ref<HTMLCanvasElement>()
const W = 128, H = 64

const SMALL = '9px "JetBrains Mono", ui-monospace, monospace'
const TINY = '7px "JetBrains Mono", ui-monospace, monospace'
const MID = '11px "JetBrains Mono", ui-monospace, monospace'
const BIG = '600 30px "Archivo", Arial, sans-serif'

const f1 = (v: number | null | undefined) => (v == null ? '--' : v.toFixed(1).replace('.', ','))
const f0 = (v: number | null | undefined) => (v == null ? '--' : v.toFixed(0))

function nowParts() {
  const d = new Date()
  const time = d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', timeZone: tz })
  const wd = d.toLocaleDateString('pt-BR', { weekday: 'short', timeZone: tz }).replace('.', '')
  const date = d.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit', year: 'numeric', timeZone: tz })
  return { time, date: `${wd}, ${date}` }
}

function draw() {
  const c = canvas.value
  if (!c) return
  const g = c.getContext('2d')!
  g.fillStyle = '#000'
  g.fillRect(0, 0, W, H)
  g.fillStyle = '#fff'
  g.strokeStyle = '#fff'
  g.textBaseline = 'alphabetic'
  const text = (s: string, x: number, y: number, font = SMALL) => {
    g.font = font
    g.fillText(s, x, y)
    return g.measureText(s).width
  }
  const width = (s: string, font = SMALL) => {
    g.font = font
    return g.measureText(s).width
  }
  const center = (s: string, y: number, font = SMALL) => text(s, Math.round((W - width(s, font)) / 2), y, font)
  const row = (y: number, label: string, value: string) => {
    text(label, 0, y)
    text(value, W - width(value), y)
  }
  const header = (title: string) => {
    const tw = text(title, 0, 8)
    const n = props.count
    const step = n > 5 ? 5 : 7, r = n > 5 ? 1 : 2
    if (n > 1) {
      for (let i = 0; i < n; i++) {
        const x = W - (n - i) * step + Math.floor(step / 2)
        g.beginPath()
        g.arc(x, 4, i === props.index ? r + (n > 5 ? 1 : 0) : r, 0, Math.PI * 2)
        if (i === props.index) g.fill()
        else g.stroke()
      }
    }
    const { time } = nowParts()
    const x = W - (n > 1 ? n * step : 0) - 3 - width(time, TINY)
    if (x > tw + 4) text(time, x, 8, TINY)
    g.fillRect(0, 11, W, 1)
  }
  const big = (value: string, unit: string) => {
    const wv = width(value, BIG), wu = width(unit, MID)
    const x = Math.round((W - (wv + 3 + wu)) / 2)
    text(value, x, 46, BIG)
    text(unit, x + wv + 3, 29, MID)
  }
  const r = props.reading

  switch (props.page.type) {
    case 'temp':
      header('Temperatura')
      big(f1(r?.temp), '°C')
      center(`Umid ${f0(r?.hum)}%  Orv ${f1(r?.dew)}°`, 63)
      break
    case 'hum':
      header('Umidade')
      big(f0(r?.hum), '%')
      center(`Ponto de orvalho ${f1(r?.dew)}°`, 63)
      break
    case 'pres':
      header('Pressão')
      big(f0(r?.pres_slp), 'hPa')
      center('Local --- hPa', 63)
      break
    case 'clock': {
      const { time, date } = nowParts()
      center(time, 34, BIG)
      center(date, 48)
      center(`${f1(r?.temp)} °C   ${f0(r?.hum)} %`, 63)
      break
    }
    case 'today': {
      const t = props.today
      header('Hoje')
      row(24, 'Mínima', `${f1(t?.temp_min)}°`)
      row(37, 'Máxima', `${f1(t?.temp_max)}°`)
      row(50, 'Umidade', `${f0(t?.hum_min)} a ${f0(t?.hum_max)} %`)
      row(63, 'Amplitude', t?.temp_max != null && t?.temp_min != null ? `${(t.temp_max - t.temp_min).toFixed(0)} °C` : '--')
      break
    }
    case 'fields': {
      header('Resumo')
      const labels: Record<FieldType, [string, string]> = {
        temp: ['Temperatura', `${f1(r?.temp)} °C`],
        hum: ['Umidade', `${f0(r?.hum)} %`],
        pres: ['Pressão', `${f0(r?.pres_slp)} hPa`],
        dew: ['Orvalho', `${f1(r?.dew)} °C`],
        heat_index: ['Sensação', '-- °C'],
        temp_bmp: ['Temp. BMP280', '-- °C'],
        pres_station: ['Pressão local', '-- hPa'],
      }
      ;(props.page.fields ?? []).slice(0, 4).forEach((f, i) => row(24 + i * 13, labels[f][0], labels[f][1]))
      break
    }
    case 'details':
      header('Detalhes')
      row(24, 'Ponto de orvalho', `${f1(r?.dew)} °C`)
      row(37, 'Temp. BMP280', '-- °C')
      row(50, 'AHT20 BMP280', 'ok ok')
      row(63, 'Ligado há', '0h00m')
      break
    case 'network':
      header('Rede')
      row(24, 'Wi-Fi', 'MinhaRede')
      row(37, 'IP', '192.168.1.105')
      row(50, 'Nuvem', 'ok')
      row(63, 'Hora', nowParts().time)
      break
  }

  // Converte para 1 bit, como o SSD1306.
  const img = g.getImageData(0, 0, W, H)
  for (let i = 0; i < img.data.length; i += 4) {
    // Aceso: azul-claro, como os OLED comuns; apagado: preto.
    const on = img.data[i]! > 110
    img.data[i] = on ? 196 : 0
    img.data[i + 1] = on ? 232 : 0
    img.data[i + 2] = on ? 255 : 0
    img.data[i + 3] = 255
  }
  g.putImageData(img, 0, 0)
}

onMounted(async () => {
  await document.fonts?.ready
  draw()
})
watch(() => [props.page, props.index, props.count, props.reading, props.today], draw, { deep: true })
let clock: ReturnType<typeof setInterval> | undefined
onMounted(() => (clock = setInterval(draw, 30_000)))
onBeforeUnmount(() => clearInterval(clock))
</script>

<template>
  <div class="oled" :class="{ dim }">
    <canvas ref="canvas" :width="W" :height="H" role="img" :aria-label="`Prévia do display: página ${PAGE_LABELS[page.type]}`" />
  </div>
</template>

<style scoped>
.oled {
  background: #05080a;
  border-radius: 10px;
  padding: 14px;
  border: 1px solid var(--line);
  box-shadow: inset 0 0 0 6px #10161a;
}
canvas {
  display: block;
  width: 100%;
  max-width: 512px;
  aspect-ratio: 2 / 1;
  image-rendering: pixelated;
  filter: drop-shadow(0 0 1px rgba(127, 211, 255, 0.45));
  background: #000;
}
.oled.dim canvas { opacity: 0.35; }
</style>
