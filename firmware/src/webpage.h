#pragma once

// Página mínima servida em "/". HTML, CSS e JS puros, sem nada externo:
// funciona sem internet. O painel completo fica no Nuxt (fase 6).
static const char WEB_INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GWeather</title>
<style>
:root{--bg:#f2f4f3;--card:#fff;--fg:#15201c;--mut:#5a6a65;--line:#d5ddd9;--acc:#a9532a;--bad:#c0352d}
@media(prefers-color-scheme:dark){:root{--bg:#0f1513;--card:#161e1b;--fg:#e2eae6;--mut:#93a49d;--line:#2a3632;--acc:#e08b5b;--bad:#ee6a62}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:640px;margin:0 auto;padding:20px 16px 40px;display:grid;gap:14px}
header{display:flex;justify-content:space-between;align-items:baseline;gap:8px;flex-wrap:wrap}
h1{margin:0;font-size:1.5rem;letter-spacing:-.01em}
.mut{color:var(--mut);font-size:.85rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;display:grid;gap:2px}
.k{color:var(--mut);font-size:.75rem;text-transform:uppercase;letter-spacing:.08em}
.v{font-size:2rem;font-weight:700;font-variant-numeric:tabular-nums}
.v small{font-size:.9rem;font-weight:500;color:var(--mut);margin-left:3px}
table{width:100%;border-collapse:collapse;background:var(--card);border:1px solid var(--line);border-radius:10px;overflow:hidden}
td{padding:8px 14px;border-bottom:1px solid var(--line);font-variant-numeric:tabular-nums}
tr:last-child td{border-bottom:0}
td:first-child{color:var(--mut)}
td:last-child{text-align:right}
.err{color:var(--bad);font-weight:600}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--acc);margin-right:6px}
</style>
</head>
<body>
<main>
  <header>
    <h1>GWeather</h1>
    <span class="mut" id="upd"><span class="dot"></span>carregando…</span>
  </header>
  <div class="grid">
    <div class="card"><span class="k">Temperatura</span><span class="v" id="temp">–</span></div>
    <div class="card"><span class="k">Umidade</span><span class="v" id="hum">–</span></div>
    <div class="card"><span class="k">Pressão (nível do mar)</span><span class="v" id="pres">–</span></div>
  </div>
  <table>
    <tr><td>Sensação térmica</td><td id="hi">–</td></tr>
    <tr><td>Ponto de orvalho</td><td id="dew">–</td></tr>
    <tr><td>Pressão no local</td><td id="pst">–</td></tr>
    <tr><td>Temperatura do BMP280</td><td id="tb">–</td></tr>
  </table>
  <table>
    <tr><td>Wi-Fi</td><td id="wifi">–</td></tr>
    <tr><td>Hora</td><td id="time">–</td></tr>
    <tr><td>Ligado há</td><td id="up">–</td></tr>
    <tr><td>Sensores</td><td id="sens">–</td></tr>
    <tr><td>Firmware</td><td id="fw">–</td></tr>
  </table>
  <p class="mut">Página local do ESP32. O painel completo, com histórico e gráficos, fica na nuvem. API: <code>/api/v1/now</code>, <code>/api/v1/status</code>, <code>/api/v1/config</code>.</p>
</main>
<script>
const $=id=>document.getElementById(id);
const n=(v,d)=>v==null?"–":v.toFixed(d).replace(".",",");
const unit=(v,d,u)=>v==null?"–":n(v,d)+"<small>"+u+"</small>";
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+"d ":"")+h+"h "+String(m).padStart(2,"0")+"m"}
async function tick(){
  try{
    const [now,st]=await Promise.all([fetch("/api/v1/now").then(r=>r.json()),fetch("/api/v1/status").then(r=>r.json())]);
    $("temp").innerHTML=unit(now.temp,1,"°C");
    $("hum").innerHTML=unit(now.hum,0,"%");
    $("pres").innerHTML=unit(now.pres_slp,0,"hPa");
    $("hi").textContent=now.heat_index==null?"–":n(now.heat_index,1)+" °C";
    $("dew").textContent=now.dew==null?"–":n(now.dew,1)+" °C";
    $("pst").textContent=now.pres_station==null?"–":n(now.pres_station,1)+" hPa";
    $("tb").textContent=now.temp_bmp==null?"–":n(now.temp_bmp,1)+" °C";
    $("wifi").textContent=st.wifi.connected?st.wifi.ssid+" · "+st.wifi.ip+" · "+st.wifi.rssi+" dBm":"desconectado";
    $("time").textContent=st.time.synced?st.time.local.replace(/^(\d+)-(\d+)-(\d+)T/,"$3/$2/$1 "):"aguardando sincronização";
    $("up").textContent=dur(st.device.uptime_s);
    const s=st.sensors;
    $("sens").innerHTML="AHT20 "+(s.aht20?"ok":'<span class="err">erro</span>')+" · BMP280 "+(s.bmp280?"ok":'<span class="err">erro</span>');
    $("fw").textContent=st.device.firmware+" · "+st.device.id;
    $("upd").innerHTML='<span class="dot"></span>leitura de '+now.age_s+" s atrás";
  }catch(e){
    $("upd").innerHTML='<span class="err">sem resposta da estação</span>';
  }
}
tick();setInterval(tick,5000);
</script>
</body>
</html>
)HTML";
