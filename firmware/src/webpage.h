#pragma once

// Página mínima servida em "/". HTML, CSS e JS puros, sem nada externo:
// funciona sem internet. O painel completo fica no Nuxt (fase 6).
// Ícone (mesmo do painel na nuvem): nuvem clara e sol cobre sobre fundo escuro.
static const char WEB_FAVICON_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="14" fill="#15201c"/><circle cx="40" cy="23" r="11" fill="#e08b5b"/><path d="M17 47h27a9.5 9.5 0 0 0 0-19 12.5 12.5 0 0 0-23.4 3.6A7.7 7.7 0 0 0 17 47z" fill="#f1f4f3" stroke="#15201c" stroke-width="3" stroke-linejoin="round"/></svg>)SVG";

static const char WEB_INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#15201c">
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
<title>GRain</title>
<style>
:root{--bg:#f2f4f3;--card:#fff;--fg:#15201c;--mut:#5a6a65;--line:#d5ddd9;--acc:#a9532a;--bad:#c0352d;--ok:#1f8a52}
@media(prefers-color-scheme:dark){:root{--bg:#0f1513;--card:#161e1b;--fg:#e2eae6;--mut:#93a49d;--line:#2a3632;--acc:#e08b5b;--bad:#ee6a62;--ok:#3cb878}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:640px;margin:0 auto;padding:20px 16px 40px;display:grid;gap:14px}
header{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap}
h1{margin:0;font-size:1.5rem;letter-spacing:-.01em;display:flex;align-items:center;gap:8px}
h1 img{width:28px;height:28px}
h2{margin:6px 0 0;font-size:1.05rem}
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
.box{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;display:grid;gap:10px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button{font:inherit;font-weight:600;border:1px solid var(--line);background:var(--bg);color:var(--fg);padding:8px 14px;border-radius:8px;cursor:pointer}
button:hover{border-color:var(--acc)}
button:disabled{opacity:.5;cursor:default}
button.on{background:var(--fg);color:var(--bg);border-color:var(--fg)}
button:focus-visible,input:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
.nav{display:grid;grid-template-columns:48px 1fr 48px;gap:8px;align-items:center}
.nav button{font-size:1.1rem;padding:8px 0}
.pg{text-align:center;display:grid}
.pg b{font-size:1.1rem}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chips button{font-size:.82rem;padding:4px 10px;border-radius:999px}
input{font:inherit;padding:7px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg);min-width:0}
.err{color:var(--bad);font-weight:600}
.okc{color:var(--ok);font-weight:600}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--acc);margin-right:6px}
.msg{min-height:1.2em}
</style>
</head>
<body>
<main>
  <header>
    <h1><img src="/favicon.svg" alt="">GRain</h1>
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

  <h2>Display</h2>
  <div class="box">
    <div class="nav">
      <button id="prev" aria-label="Página anterior">◀</button>
      <div class="pg"><b id="pgname">–</b><span class="mut" id="pgpos"></span></div>
      <button id="next" aria-label="Próxima página">▶</button>
    </div>
    <div class="chips" id="chips"></div>
    <div class="row">
      <button id="pause">Pausar carrossel</button>
      <button id="power">Desligar tela</button>
    </div>
    <span class="mut" id="dst"></span>
  </div>

  <h2>Nuvem</h2>
  <div class="box">
    <table>
      <tr><td>Último envio</td><td id="c_ok">–</td></tr>
      <tr><td>Médias na fila</td><td id="c_q">–</td></tr>
      <tr><td>Configuração do painel</td><td id="c_cfg">–</td></tr>
    </table>
    <div class="row"><button id="sync">Sincronizar agora</button><span class="mut msg" id="syncmsg"></span></div>
  </div>

  <table>
    <tr><td>Wi-Fi</td><td id="wifi">–</td></tr>
    <tr><td>Hora</td><td id="time">–</td></tr>
    <tr><td>Ligado há</td><td id="up">–</td></tr>
    <tr><td>Memória livre</td><td id="mem">–</td></tr>
    <tr><td>Sensores</td><td id="sens">–</td></tr>
    <tr><td>Firmware</td><td id="fw">–</td></tr>
  </table>

  <h2>Manutenção</h2>
  <div class="box">
    <div class="row">
      <label for="pw" class="mut">Senha da estação</label>
      <input id="pw" type="password" autocomplete="current-password" placeholder="comando status no console">
      <button id="reboot">Reiniciar</button>
    </div>
    <span class="mut msg" id="rbmsg"></span>
  </div>

  <p class="mut">Página local do ESP32, funciona sem internet. Histórico, gráficos e configuração do display ficam no painel na nuvem. API: <code>/api/v1/now</code>, <code>/status</code>, <code>/display</code>, <code>/config</code>.</p>
</main>
<script>
const $=id=>document.getElementById(id);
const NAMES={temp:"Temperatura",hum:"Umidade",pres:"Pressão",clock:"Relógio",today:"Hoje",fields:"Resumo",details:"Detalhes",network:"Rede"};
const n=(v,d)=>v==null?"–":v.toFixed(d).replace(".",",");
const unit=(v,d,u)=>v==null?"–":n(v,d)+"<small>"+u+"</small>";
const hm=e=>e?new Date(e*1000).toLocaleTimeString("pt-BR",{hour:"2-digit",minute:"2-digit"}):"nunca";
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+"d ":"")+h+"h "+String(m).padStart(2,"0")+"m"}
let disp=null;
async function post(url,body,headers){
  const r=await fetch(url,{method:"POST",headers:Object.assign({"Content-Type":"application/json"},headers||{}),body:JSON.stringify(body||{})});
  const j=await r.json().catch(()=>({}));
  if(!r.ok)throw new Error(j.detail||j.error||("HTTP "+r.status));
  return j;
}
function renderDisplay(d){
  disp=d;
  const name=NAMES[d.pages[d.page]]||d.pages[d.page];
  $("pgname").textContent=d.on?name:"Tela desligada";
  $("pgpos").textContent="página "+(d.page+1)+" de "+d.pages.length;
  const c=$("chips");c.innerHTML="";
  d.pages.forEach((p,i)=>{const b=document.createElement("button");b.textContent=(i+1)+". "+(NAMES[p]||p);if(i===d.page)b.className="on";b.onclick=()=>act({page:i});c.appendChild(b)});
  $("pause").textContent=d.paused?"Retomar carrossel":"Pausar carrossel";
  $("pause").classList.toggle("on",d.paused);
  $("power").textContent=d.on?"Desligar tela":"Ligar tela";
  $("power").classList.toggle("on",!d.on);
  let st=[];
  if(!d.on)st.push("tela desligada até religar ou reiniciar");
  else if(d.night)st.push("modo noturno ativo");
  if(d.paused)st.push("carrossel pausado");
  else if(d.hold_s)st.push("volta a girar em "+d.hold_s+" s");
  else st.push("trocando a cada "+d.page_s+" s");
  $("dst").textContent=st.join(" · ");
}
async function act(body){try{renderDisplay(await post("/api/v1/display",body))}catch(e){$("dst").innerHTML='<span class="err">'+e.message+"</span>"}}
$("prev").onclick=()=>act({page:"prev"});
$("next").onclick=()=>act({page:"next"});
$("pause").onclick=()=>disp&&act({paused:!disp.paused});
$("power").onclick=()=>disp&&act({power:!disp.on});
$("sync").onclick=async()=>{
  $("sync").disabled=true;$("syncmsg").textContent="sincronizando…";
  try{await post("/api/v1/cloud/sync");setTimeout(async()=>{await tick();$("syncmsg").innerHTML='<span class="okc">feito</span>';$("sync").disabled=false},4000)}
  catch(e){$("syncmsg").innerHTML='<span class="err">'+e.message+"</span>";$("sync").disabled=false}
};
$("reboot").onclick=async()=>{
  const pw=$("pw").value;if(!pw){$("rbmsg").innerHTML='<span class="err">Digite a senha da estação.</span>';return}
  try{await post("/api/v1/system/reboot",{},{Authorization:"Basic "+btoa("admin:"+pw)});$("rbmsg").textContent="Reiniciando… a página volta em cerca de 15 s.";$("pw").value=""}
  catch(e){$("rbmsg").innerHTML='<span class="err">'+(e.message.includes("401")?"Senha incorreta.":e.message)+"</span>"}
};
async function tick(){
  try{
    const [now,st,d]=await Promise.all(["/api/v1/now","/api/v1/status","/api/v1/display"].map(u=>fetch(u).then(r=>r.json())));
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
    $("mem").textContent=Math.round(st.device.heap_free/1024)+" KB";
    const s=st.sensors;
    $("sens").innerHTML="AHT20 "+(s.aht20?"ok":'<span class="err">erro</span>')+" · BMP280 "+(s.bmp280?"ok":'<span class="err">erro</span>');
    $("fw").textContent=st.device.firmware+" · "+st.device.id;
    const c=st.cloud;
    if(!c.configured){$("c_ok").textContent="nuvem não configurada";$("sync").disabled=true}
    else{$("c_ok").innerHTML=c.last_error?'<span class="err">'+c.last_error+"</span>":(c.last_ok?hm(c.last_ok):"ainda não (envia a cada 10 min)");}
    $("c_q").textContent=c.queue;
    $("c_cfg").textContent="versão "+c.config_version+(c.last_config?" · consultada às "+hm(c.last_config):"");
    renderDisplay(d);
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
