#pragma once

// Página local servida em "/". HTML, CSS e JS puros, sem nada externo:
// funciona sem internet. Aqui ficam todas as configurações da estação;
// o painel na nuvem (Nuxt) só mostra dados e uma cópia da configuração.
// Ícone (mesmo do painel na nuvem): nuvem clara e sol cobre sobre fundo escuro.
static const char WEB_FAVICON_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="14" fill="#15201c"/><circle cx="40" cy="23" r="11" fill="#e08b5b"/><path d="M17 47h27a9.5 9.5 0 0 0 0-19 12.5 12.5 0 0 0-23.4 3.6A7.7 7.7 0 0 0 17 47z" fill="#f1f4f3" stroke="#15201c" stroke-width="3" stroke-linejoin="round"/></svg>)SVG";

static const char WEB_INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0a1013">
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
<title>GWethr · estação local</title>
<style>
/* App local da estação: cara de aparelho (moldura escura, tipografia mono, ciano do OLED),
   para não confundir com o painel na nuvem (fonte Archivo, cobre, navegação no topo). */
:root{--bg:#eef3f4;--card:#fff;--fg:#0f1a1d;--mut:#55686d;--line:#cfdcdf;--acc:#0b7d8c;--bad:#c0352d;--ok:#1f8a52;--warn:#b77306;
  --bezel:#0a1013;--bezel2:#131c20;--oled:#7fe3f0;--oledd:#2c5d66;--mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
@media(prefers-color-scheme:dark){:root{--bg:#0c1214;--card:#141d20;--fg:#e1eaec;--mut:#8ea2a7;--line:#25343a;--acc:#4fd0e0;--bad:#ee6a62;--ok:#3cb878;--warn:#e3a33a}}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding-bottom:calc(76px + env(safe-area-inset-bottom))}
.bezel{background:var(--bezel);color:#cfe3e7;padding:14px 16px 12px;padding-top:calc(14px + env(safe-area-inset-top));border-bottom:3px solid var(--acc)}
.bezel>div{max-width:640px;margin:0 auto;display:grid;gap:10px}
.brand{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.brand img{width:30px;height:30px}
.brand h1{margin:0;font:700 1.25rem var(--mono);letter-spacing:.02em;color:#fff}
.badge{font:700 .66rem var(--mono);letter-spacing:.12em;background:var(--oled);color:var(--bezel);padding:3px 7px;border-radius:4px}
.where{font:.74rem var(--mono);color:#8fb0b7;margin-left:auto}
.oled{background:#000;border:1px solid #1f2d33;border-radius:6px;padding:7px 10px;display:flex;justify-content:space-between;gap:10px;font:.78rem var(--mono);color:var(--oled);text-shadow:0 0 6px rgba(127,227,240,.35)}
.oled span:last-child{color:var(--oledd)}
main{max-width:640px;margin:0 auto;padding:16px 16px 24px;display:grid;gap:14px}
.tab{display:none;gap:14px}.tab.on{display:grid}
h2{margin:6px 0 0;font:700 .78rem var(--mono);letter-spacing:.12em;text-transform:uppercase;color:var(--mut)}
h3{margin:4px 0 0;font:700 .9rem var(--mono)}
.mut{color:var(--mut);font-size:.85rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;display:grid;gap:2px}
.k{color:var(--mut);font:.7rem var(--mono);text-transform:uppercase;letter-spacing:.1em}
.v{font:700 2rem var(--mono);font-variant-numeric:tabular-nums;letter-spacing:-.02em}
.v small{font:500 .85rem system-ui;color:var(--mut);margin-left:3px}
table{width:100%;border-collapse:collapse;background:var(--card);border:1px solid var(--line);border-radius:10px;overflow:hidden}
td{padding:8px 14px;border-bottom:1px solid var(--line);font-variant-numeric:tabular-nums}
tr:last-child td{border-bottom:0}
td:first-child{color:var(--mut)}
td:last-child{text-align:right}
.box{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;display:grid;gap:10px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button{font:600 .92rem system-ui;border:1px solid var(--line);background:var(--bg);color:var(--fg);padding:8px 14px;border-radius:8px;cursor:pointer}
button:hover{border-color:var(--acc)}
button:disabled{opacity:.5;cursor:default}
button.on{background:var(--fg);color:var(--bg);border-color:var(--fg)}
button.pri{background:var(--acc);border-color:var(--acc);color:#fff}
@media(prefers-color-scheme:dark){button.pri{color:#04181b}}
button:focus-visible,input:focus-visible,select:focus-visible,a:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
input,select{font:inherit;padding:7px 9px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg);min-width:0}
.pl{list-style:none;margin:0;padding:0;display:grid;gap:6px}
.pl li{display:grid;grid-template-columns:22px 1fr auto;gap:8px;align-items:center;padding:6px 8px;border:1px solid var(--line);border-radius:8px;background:var(--bg)}
.pl li>b{font:.85rem var(--mono);color:var(--mut);text-align:center}
.pl .tools{display:flex;gap:4px}
.pl .tools button{padding:4px 9px}
.fs{grid-column:2/4;display:flex;flex-wrap:wrap;gap:4px 12px;font-size:.85rem}
.f2,.f3{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(140px,1fr))}
.f4{grid-template-columns:repeat(auto-fit,minmax(110px,1fr))}
.f2 label,.f3 label{display:grid;gap:3px;font-size:.8rem;color:var(--mut)}
label.ck{display:flex!important;align-items:center;gap:8px;font-size:.95rem!important;color:var(--fg)!important}
.f2 input,.f3 input,.f2 select{width:100%}
input[type=range]{padding:0;accent-color:var(--acc)}
input[type=checkbox]{width:18px;height:18px;accent-color:var(--acc)}
.nav{display:grid;grid-template-columns:48px 1fr 48px;gap:8px;align-items:center}
.nav button{font-size:1.1rem;padding:8px 0}
.pg{text-align:center;display:grid}
.pg b{font:700 1.05rem var(--mono)}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chips button{font-size:.82rem;padding:4px 10px;border-radius:999px}
.err{color:var(--bad);font-weight:600}.ok{color:var(--ok)}
.okc{color:var(--ok);font-weight:600}
.msg{min-height:1.2em}
.rt{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.rt div{border:1px solid var(--line);border-radius:8px;padding:8px 10px;display:grid;gap:3px;background:var(--bg)}
.rt b{font:700 1.6rem var(--mono);font-variant-numeric:tabular-nums}
.rt i{display:block;height:5px;border-radius:3px;background:var(--line);overflow:hidden}
.rt i span{display:block;height:100%;background:var(--mut)}
.rt .m i span{background:var(--warn)}.rt .a i span{background:var(--bad)}.rt .a{border-color:var(--bad)}
.mt{width:100%;border-collapse:collapse;font-size:.85rem;background:transparent;border:0}
.mt th,.mt td{padding:5px 6px;border-bottom:1px solid var(--line);text-align:right;font-variant-numeric:tabular-nums}
.mt th{font:600 .68rem var(--mono);letter-spacing:.08em;text-transform:uppercase;color:var(--mut)}
.mt td:first-child,.mt th:first-child{text-align:left;color:var(--fg)}
.mt td small,#t-sistema td small{display:block;color:var(--mut);font-size:.72rem}
.mt tr:last-child td{border-bottom:0}
.mt td.best{font-weight:700;box-shadow:inset 0 -2px 0 var(--acc)}
.alert{background:var(--bad);color:#fff;border-radius:8px;padding:8px 12px;font-weight:700}
.save{position:sticky;bottom:calc(70px + env(safe-area-inset-bottom));background:var(--card);border:1px solid var(--line);border-radius:12px;padding:10px 12px;display:grid;gap:8px;box-shadow:0 6px 20px rgba(0,0,0,.12)}
.save.dirty{border-color:var(--warn)}
.save input{flex:1}
.tabs{position:fixed;left:0;right:0;bottom:0;background:var(--bezel);border-top:1px solid #1f2d33;padding-bottom:env(safe-area-inset-bottom);z-index:5}
.tabs div{max-width:640px;margin:0 auto;display:grid;grid-template-columns:repeat(4,1fr)}
.tabs a{display:grid;justify-items:center;gap:2px;padding:9px 4px 8px;color:#8fb0b7;text-decoration:none;font:600 .68rem var(--mono);letter-spacing:.08em;text-transform:uppercase;border-top:2px solid transparent}
.tabs a i{font-style:normal;font-size:1.15rem;line-height:1}
.tabs a.on{color:var(--oled);border-top-color:var(--oled)}
.foot{font-size:.8rem;color:var(--mut);text-align:center}
.foot a{color:var(--acc)}
</style>
</head>
<body>
<header class="bezel"><div>
  <div class="brand"><img src="/favicon.svg" alt=""><h1>GWethr</h1><span class="badge">APP LOCAL · ESP32</span><span class="where" id="where">gwethr.local</span></div>
  <div class="oled"><span id="oled" aria-live="polite">conectando à estação…</span><span><span id="osend"></span> · <span id="upd">–</span></span></div>
</div></header>
<main>
  <section class="tab" id="t-agora">
    <div id="ralert" class="alert" hidden></div>
    <h2>Chuva nas próximas horas</h2>
    <div class="box">
      <div class="rt" id="rt"><div><span class="mut">1 h</span><b>–</b><i><span></span></i><span class="mut">–</span></div><div><span class="mut">2 h</span><b>–</b><i><span></span></i><span class="mut">–</span></div><div><span class="mut">3 h</span><b>–</b><i><span></span></i><span class="mut">–</span></div></div>
      <span class="mut"><span id="rwhy"></span><span id="rnext"></span></span>
      <table class="mt" id="mt" hidden>
        <thead><tr><th>Os cinco métodos</th><th>1 h</th><th>2 h</th><th>3 h</th></tr></thead>
        <tbody>
          <tr><td>M1<small>só o barômetro da estação</small></td><td></td><td></td><td></td></tr>
          <tr><td>M2<small>barômetro + aeroporto (METAR e TAF)</small></td><td></td><td></td><td></td></tr>
          <tr><td>M2T<small>o M2 com pesos aprendidos</small></td><td></td><td></td><td></td></tr>
          <tr><td>M3<small>modelo numérico (Open-Meteo)</small></td><td></td><td></td><td></td></tr>
          <tr><td>M4<small>combinado, pesos aprendidos</small></td><td></td><td></td><td></td></tr>
        </tbody>
      </table>
      <span class="mut" id="mthelp" hidden>Os mostradores acima usam o método em destaque (sublinhado): o modelo, até o placar ter dados; depois, o que mais acertou em cada janela. Detalhes e placar no painel, aba Chuva.</span>
      <div class="row"><button id="rrep" title="ou segure o botão da estação por 2 s">☂ Está chovendo aqui</button><button id="rnone">☀ Não choveu hoje</button><span class="mut msg" id="rmsg"></span></div>
    </div>
    <h2>Sensores agora</h2>
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
  </section>

  <section class="tab" id="t-display">
    <h2>Controle agora</h2>
    <div class="box">
      <div class="nav">
        <button id="prev" aria-label="Página anterior">◀</button>
        <div class="pg"><b id="pgname">–</b><span class="mut" id="pgpos"></span></div>
        <button id="next" aria-label="Próxima página">▶</button>
      </div>
      <div class="chips" id="chips"></div>
      <div class="row"><button id="power">Desligar tela</button><span class="mut" id="dst"></span></div>
      <span class="mut">Botão da estação: toque troca de página (com a tela apagada, acende por 15 s; com alerta, cala); segure 2 s para “Está chovendo aqui”; segure 5 s para desligar a tela, ou para mantê-la ligada quando estiver apagada.</span>
    </div>
    <h2>Páginas</h2>
    <div class="box">
      <ol class="pl" id="pl"></ol>
      <div class="row"><select id="newp" aria-label="Tipo da nova página"></select><button id="addp">Adicionar página</button><span class="mut" id="pcount"></span></div>
      <div class="f2">
        <label>Troca de página<select id="b_mode"><option value="button">pelo botão</option><option value="auto">automática</option><option value="both">automática e pelo botão</option></select></label>
        <label id="ps_l">Tempo por página (s)<input id="page_s" type="number" min="2" max="60" step="1"></label>
        <label id="bd_l">Escurecer sem toque (min)<input id="b_dim" type="number" min="0" max="120" step="1" title="0 = nunca"></label>
        <label>Brilho <span id="ctv"></span><input id="contrast" type="range" min="0" max="255" step="5"></label>
      </div>
      <span class="mut" id="modehelp"></span>
    </div>
    <h2>Modo noturno</h2>
    <div class="box">
      <div class="f2 f4">
        <label class="ck"><input id="n_on" type="checkbox">Ligado</label>
        <label>Durante a noite<select id="n_mode"><option value="dim">escurecer</option><option value="off">apagar</option></select></label>
        <label>Das<input id="n_from" type="time"></label>
        <label>Às<input id="n_to" type="time"></label>
      </div>
    </div>
    <h2>Alerta de chuva</h2>
    <div class="box">
      <span class="mut">Quando a chance passa do limite, o buzzer dá 3 bipes curtos, o display pisca, mostra um guarda-chuva no cabeçalho e uma tela de alerta (na troca automática, ela volta a cada duas páginas) e o LED azul pisca duplo. Um toque no botão cala; volta se a chance subir 15 pontos.</span>
      <div class="f2 f4">
        <label class="ck"><input id="a_on" type="checkbox">Ligado</label>
        <label>A partir de (%)<input id="a_pct" type="number" min="10" max="100" step="5"></label>
        <label>Nas próximas<select id="a_h"><option value="1">1 hora</option><option value="2">2 horas</option><option value="3">3 horas</option></select></label>
        <label>Mostrar<select id="a_mode"><option value="both">ícone e tela</option><option value="icon">só o ícone</option><option value="screen">só a tela</option></select></label>
      </div>
      <div class="row"><button id="a_test">Testar alerta</button><span class="mut">mostra o alerta por 12 s com valores de exemplo (com bipe)</span></div>
    </div>
    <h2>Buzzer e LED</h2>
    <div class="box">
      <div class="f2 f4">
        <label>LED azul<select id="d_led"><option value="normal">normal</option><option value="warnings">só avisos</option><option value="off">desligado</option></select></label>
        <label>LED no silencioso<select id="d_ledq"><option value="warnings">só avisos</option><option value="normal">normal</option><option value="off">desligado</option></select></label>
        <label class="ck"><input id="z_on" type="checkbox">Buzzer ligado</label>
        <label class="ck"><input id="z_click" type="checkbox">Clique nos toques</label>
        <label>O buzzer liga com<select id="z_lvl"><option value="high">nível alto</option><option value="low">nível baixo</option></select></label>
        <label class="ck"><input id="q_on" type="checkbox">Horário silencioso</label>
        <span class="mut">buzzer calado e LED como acima</span>
        <label>Das<input id="q_from" type="time"></label>
        <label>Às<input id="q_to" type="time"></label>
      </div>
      <div class="row"><button id="z_test">Testar buzzer</button><span class="mut">3 bipes, mesmo no silencioso. Se o buzzer apitar sem parar ao ligar, troque “O buzzer liga com” e salve.</span></div>
    </div>
  </section>

  <section class="tab" id="t-ajustes">
    <h2>Calibração</h2>
    <div class="box">
      <label class="ck"><input id="c_auto" type="checkbox">Calibrar a pressão pelo aeroporto mais próximo</label>
      <span class="mut" id="c_autotxt"></span>
      <span class="mut">Ajustes manuais: valor somado a cada leitura.</span>
      <div class="f3">
        <label>Temperatura (°C)<input id="c_t" type="number" min="-5" max="5" step="0.1"></label>
        <label>Umidade (%)<input id="c_h" type="number" min="-15" max="15" step="0.5"></label>
        <label>Pressão (hPa)<input id="c_p" type="number" min="-15" max="15" step="0.1"></label>
      </div>
    </div>
    <h2>Localização</h2>
    <div class="box">
      <span class="mut">Para os dados de fora (aeroporto mais próximo e previsão). No Google Maps, toque e segure no local, copie os números e cole na latitude.</span>
      <div class="f3">
        <label>Latitude<input id="l_lat" inputmode="decimal" placeholder="-23.5"></label>
        <label>Longitude<input id="l_lon" inputmode="decimal" placeholder="-46.6"></label>
        <label>Nome do local<input id="l_lab" maxlength="40" placeholder="Casa"></label>
      </div>
    </div>
  </section>

  <section class="tab" id="t-sistema">
    <h2>Testes em andamento</h2>
    <table>
      <tr><td>Robustez (72 h)<small>sem reinícios, sem buracos, memória estável</small></td><td id="x_h72">–</td></tr>
      <tr><td>Chuva (30 dias)<small>M2T e M4 no placar</small></td><td id="x_d30">–</td></tr>
    </table>
    <h2>Nuvem</h2>
    <div class="box">
      <table>
        <tr><td>Último envio</td><td id="c_ok">–</td></tr>
        <tr><td>Médias na fila</td><td id="c_q">–</td></tr>
        <tr><td>Cópia da configuração</td><td id="c_cfg">–</td></tr>
      </table>
      <div class="row"><button id="sync">Sincronizar agora</button><a id="cloud" href="#" target="_blank" rel="noopener" hidden>Abrir o painel na nuvem ↗</a><span class="mut msg" id="syncmsg"></span></div>
      <div class="f2">
        <label>Endereço do painel<input id="c_url" type="url" inputmode="url" placeholder="https://..."></label>
        <label>Senha da estação<input id="pw_url" type="password" autocomplete="current-password"></label>
      </div>
      <div class="row"><button id="urlsave">Trocar endereço</button><span class="mut msg" id="urlmsg"></span></div>
    </div>
    <h2>Próximas atualizações</h2>
    <table>
      <tr><td>Leitura dos sensores</td><td id="n_read">a cada 5 s</td></tr>
      <tr><td>Esta página</td><td id="n_page">–</td></tr>
      <tr><td>Envio da média para a nuvem</td><td id="n_send">–</td></tr>
      <tr><td>Previsão de chuva (vem com o envio)</td><td id="n_rain">–</td></tr>
      <tr><td>Cópia da configuração</td><td>na hora, quando algo muda</td></tr>
    </table>
    <h2>Estação</h2>
    <table>
      <tr><td>Wi-Fi</td><td id="wifi">–</td></tr>
      <tr><td>Hora</td><td id="time">–</td></tr>
      <tr><td>Ligado há</td><td id="up">–</td></tr>
      <tr><td>Memória livre</td><td id="mem">–</td></tr>
      <tr><td>Sensores</td><td id="sens">–</td></tr>
      <tr><td>Firmware</td><td id="fw">–</td></tr>
      <tr><td>Último reinício</td><td id="rst">–</td></tr>
      <tr><td>Botão</td><td id="btn">–</td></tr>
      <tr><td>Reinícios registrados</td><td id="boots">–</td></tr>
      <tr><td>Menor memória livre</td><td id="memmin">–</td></tr>
      <tr><td>Atualização pelo Wi-Fi</td><td id="ota">–</td></tr>
    </table>
    <h2>Senha da estação</h2>
    <div class="box">
      <span class="mut">Usada para salvar configurações, reiniciar e entrar na rede de configuração do Wi-Fi (GWethr-Setup). De 8 a 63 caracteres, sem espaços nem acentos.</span>
      <div class="f3">
        <label>Senha atual<input id="pw_cur" type="password" autocomplete="current-password"></label>
        <label>Nova senha<input id="pw_new" type="password" autocomplete="new-password" minlength="8" maxlength="63"></label>
        <label>Repita a nova<input id="pw_new2" type="password" autocomplete="new-password" minlength="8" maxlength="63"></label>
      </div>
      <div class="row"><button id="pwchg">Trocar senha</button><span class="mut msg" id="pwmsg"></span></div>
    </div>
    <h2>Manutenção</h2>
    <div class="box">
      <div class="row"><input id="pw_rb" type="password" autocomplete="current-password" placeholder="senha da estação"><button id="reboot">Reiniciar</button></div>
      <span class="mut msg" id="rbmsg"></span>
    </div>
  </section>

  <div class="save" id="savebar" hidden>
    <span class="msg mut" id="savemsg">As configurações ficam gravadas na estação; o painel na nuvem recebe uma cópia.</span>
    <div class="row"><input id="pw" type="password" autocomplete="current-password" placeholder="senha da estação (comando status)"><button id="save" class="pri">Salvar</button><button id="undo">Desfazer</button></div>
  </div>

  <p class="foot">Este é o app <b>local</b> da estação: roda no ESP32 e funciona sem internet. Histórico e gráficos ficam no painel na nuvem.</p>
</main>
<nav class="tabs" aria-label="Seções"><div>
  <a href="#agora" data-t="agora"><i>☂</i>Agora</a>
  <a href="#display" data-t="display"><i>▣</i>Display</a>
  <a href="#ajustes" data-t="ajustes"><i>⚙</i>Ajustes</a>
  <a href="#sistema" data-t="sistema"><i>ⓘ</i>Sistema</a>
</div></nav>
<script>
const $=id=>document.getElementById(id);
const NAMES={temp:"Temperatura",hum:"Umidade",pres:"Pressão",clock:"Relógio",today:"Hoje",fields:"Resumo",details:"Detalhes",network:"Rede",rain:"Chuva"};
const n=(v,d)=>v==null?"–":v.toFixed(d).replace(".",",");
const unit=(v,d,u)=>v==null?"–":n(v,d)+"<small>"+u+"</small>";
const hm=e=>e?new Date(e*1000).toLocaleTimeString("pt-BR",{hour:"2-digit",minute:"2-digit"}):"nunca";
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+"d ":"")+h+"h "+String(m).padStart(2,"0")+"m"}
let disp=null;
const FN={temp:"Temperatura",hum:"Umidade",pres:"Pressão",dew:"Orvalho",heat_index:"Sensação",temp_bmp:"Temp. BMP280",pres_station:"Pressão no local"};
const say=(id,t,bad)=>{$(id).innerHTML='<span class="'+(bad?"err":"okc")+'">'+t+"</span>"};
const auth=pw=>"Basic "+btoa("admin:"+pw);
const num=v=>{v=String(v).trim().replace(",",".");return v===""?null:Number(v)};
const r1=(v,d)=>v==null?"":String(Math.round(v*d)/d);
let P=[];
function el(tag,props,...kids){const e=Object.assign(document.createElement(tag),props||{});e.append(...kids);return e}
function drawPages(){
  const ol=$("pl");ol.innerHTML="";
  P.forEach((p,i)=>{
    const sel=el("select",{"ariaLabel":"Tipo da página "+(i+1)});
    for(const k in NAMES)sel.add(new Option(NAMES[k],k));
    sel.value=p.type;
    sel.onchange=()=>{p.type=sel.value;if(p.type==="fields"&&!(p.fields&&p.fields.length))p.fields=["temp","hum","pres","dew"];drawPages()};
    const tools=el("span",{className:"tools"});
    [["↑",-1],["↓",1]].forEach(([t,d])=>tools.append(el("button",{textContent:t,disabled:i+d<0||i+d>=P.length,onclick:()=>{[P[i],P[i+d]]=[P[i+d],P[i]];drawPages();markDirty()}})));
    tools.append(el("button",{textContent:"✕",disabled:P.length<2,onclick:()=>{P.splice(i,1);drawPages();markDirty()}}));
    const li=el("li",{},el("b",{textContent:i+1}),sel,tools);
    if(p.type==="fields"){
      const fs=el("div",{className:"fs"});
      for(const k in FN){
        const c=el("input",{type:"checkbox",checked:p.fields.includes(k)});
        c.disabled=!c.checked&&p.fields.length>=4;
        c.onchange=()=>{const set=new Set(p.fields);c.checked?set.add(k):set.delete(k);if(set.size)p.fields=Object.keys(FN).filter(f=>set.has(f));drawPages()};
        fs.append(el("label",{},c," "+FN[k]));
      }
      li.append(fs);
    }
    ol.append(li);
  });
  $("pcount").textContent=P.length+" de 8";$("addp").disabled=P.length>=8;
}
const ct=()=>$("ctv").textContent=Math.round($("contrast").value/2.55)+"%";
function autoTxt(o){const on=$("c_auto").checked;$("c_p").disabled=on;
  $("c_autotxt").textContent=on?(o?"Ajuste atual: "+(o>0?"+":"")+n(o,2)+" hPa (mediana de 7 dias, recalculada a cada envio). O ajuste manual de pressão fica desligado.":"Aguardando a nuvem calcular o ajuste (precisa de cerca de 12 h de dados do aeroporto)."):"Desligado: vale o ajuste manual de pressão."}
$("c_auto").onchange=()=>autoTxt(null);
async function loadCfg(){
  const c=await fetch("/api/v1/config").then(r=>r.json());
  const d=c.display,l=c.location||{},k=c.calibration;
  P=d.pages.map(p=>({type:p.type,fields:(p.fields||[]).slice()}));
  $("page_s").value=d.page_s;$("contrast").value=d.contrast;ct();
  $("n_on").checked=d.night.enabled;$("n_mode").value=d.night.mode;$("n_from").value=d.night.from;$("n_to").value=d.night.to;
  $("c_t").value=r1(k.temp,10);$("c_h").value=r1(k.hum,10);$("c_p").value=r1(k.pres,10);
  $("c_auto").checked=k.pres_auto!==false;autoTxt(k.pres_auto_offset);
  pmode=d.page_mode||"auto";$("b_mode").value=pmode;$("b_dim").value=d.idle_dim_min??10;modeUI();
  const z=c.buzzer||{},q=c.quiet||{},lg=c.led||{};
  $("z_on").checked=z.enabled!==false;$("z_click").checked=z.clicks!==false;$("z_lvl").value=z.level||"high";
  $("q_on").checked=q.enabled!==false;$("q_from").value=q.from||"22:00";$("q_to").value=q.to||"07:00";
  $("d_led").value=lg.mode||"normal";$("d_ledq").value=lg.quiet||"warnings";
  const a=c.rain_alert||{};$("a_on").checked=a.enabled!==false;$("a_pct").value=a.threshold||60;$("a_h").value=String(a.hours||2);$("a_mode").value=a.mode||"both";
  $("l_lat").value=r1(l.lat,1e5);$("l_lon").value=r1(l.lon,1e5);$("l_lab").value=l.label||"";
  drawPages();
  dirty=false;$("savebar").classList.remove("dirty");
}
// Troca de página: mostra só as opções que valem no modo escolhido.
function modeUI(){const m=$("b_mode").value;$("ps_l").hidden=m==="button";$("bd_l").hidden=m!=="button";
  $("modehelp").textContent=m==="button"?"A página só muda com o toque no botão (ou pelos controles acima). Sem toque, a tela escurece depois do tempo escolhido e a imagem se desloca 1 px de tempos em tempos, para não marcar o OLED.":m==="both"?"As páginas trocam sozinhas; um toque no botão avança e segura a página por 30 s.":"As páginas trocam sozinhas; um toque no botão avança e segura a página por 60 s."}
$("b_mode").onchange=modeUI;
for(const k in NAMES)$("newp").add(new Option(NAMES[k],k));
$("addp").onclick=()=>{const t=$("newp").value;P.push(t==="fields"?{type:t,fields:["temp","hum","pres","dew"]}:{type:t,fields:[]});drawPages();markDirty()};
$("contrast").oninput=ct;
$("l_lat").oninput=e=>{const m=e.target.value.match(/^\s*(-?\d+\.\d+)\s*,\s*(-?\d+\.\d+)\s*$/);if(m){$("l_lat").value=m[1];$("l_lon").value=m[2]}};
// Abas (lembradas no endereço, ex.: gwethr.local/#display) e barra de salvar nas abas de configuração.
const TABS=["agora","display","ajustes","sistema"];
function showTab(){const t=TABS.includes(location.hash.slice(1))?location.hash.slice(1):"agora";
  TABS.forEach(x=>{$("t-"+x).classList.toggle("on",x===t)});
  document.querySelectorAll(".tabs a").forEach(a=>a.classList.toggle("on",a.dataset.t===t));
  $("savebar").hidden=!(t==="display"||t==="ajustes")}
addEventListener("hashchange",showTab);showTab();
let dirty=false;
function markDirty(){dirty=true;$("savebar").classList.add("dirty");$("savemsg").innerHTML='<span class="err" style="color:var(--warn)">Alterações não salvas</span>'}
["t-display","t-ajustes"].forEach(id=>{$(id).addEventListener("input",e=>{if(!e.target.closest(".nav,.chips"))markDirty()});$(id).addEventListener("change",e=>{if(!e.target.closest(".nav,.chips"))markDirty()})});
addEventListener("beforeunload",e=>{if(dirty){e.preventDefault();e.returnValue=""}});
$("undo").onclick=()=>{loadCfg();$("savemsg").textContent="Alterações desfeitas."};
$("save").onclick=async()=>{
  const pw=$("pw").value;if(!pw)return say("savemsg","Digite a senha da estação.",1);
  const lat=num($("l_lat").value),lon=num($("l_lon").value);
  if((lat==null)!==(lon==null))return say("savemsg","Preencha latitude e longitude, ou deixe as duas vazias.",1);
  const body={
    display:{page_s:+$("page_s").value,contrast:+$("contrast").value,
      night:{enabled:$("n_on").checked,from:$("n_from").value,to:$("n_to").value,mode:$("n_mode").value},
      page_mode:$("b_mode").value,idle_dim_min:+$("b_dim").value||0,
      pages:P.map(p=>p.type==="fields"?{type:"fields",fields:p.fields}:{type:p.type})},
    calibration:{temp:num($("c_t").value)||0,hum:num($("c_h").value)||0,pres:num($("c_p").value)||0,pres_auto:$("c_auto").checked},
    location:{lat:lat,lon:lon,label:$("l_lab").value.trim()},
    rain_alert:{enabled:$("a_on").checked,threshold:+$("a_pct").value,hours:+$("a_h").value,mode:$("a_mode").value},
    buzzer:{enabled:$("z_on").checked,clicks:$("z_click").checked,level:$("z_lvl").value},
    led:{mode:$("d_led").value,quiet:$("d_ledq").value},
    quiet:{enabled:$("q_on").checked,from:$("q_from").value,to:$("q_to").value}};
  $("save").disabled=true;
  try{
    const r=await fetch("/api/v1/config",{method:"PUT",headers:{"Content-Type":"application/json",Authorization:auth(pw)},body:JSON.stringify(body)});
    const j=await r.json().catch(()=>({}));
    if(r.status===401)throw new Error("Senha incorreta.");
    if(!r.ok)throw new Error(j.detail||j.error||("HTTP "+r.status));
    await loadCfg();tick();
    say("savemsg","Salvo. O display já mudou; a cópia vai para o painel em instantes.");
  }catch(e){say("savemsg",e.message,1)}
  finally{$("save").disabled=false}
};
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
  $("power").textContent=d.on?"Desligar tela":"Ligar tela";
  $("power").classList.toggle("on",!d.on);
  let st=[];
  if(!d.on)st.push("tela desligada até religar ou reiniciar");
  else if(d.night)st.push("modo noturno ativo");
  if(pmode==="button")st.push("troca pelo botão");
  else if(d.paused)st.push("troca automática parada");
  else if(d.hold_s)st.push("volta a trocar sozinha em "+d.hold_s+" s");
  else st.push("trocando a cada "+d.page_s+" s");
  $("dst").textContent=st.join(" · ");
}
async function act(body){try{renderDisplay(await post("/api/v1/display",body))}catch(e){$("dst").innerHTML='<span class="err">'+e.message+"</span>"}}
$("prev").onclick=()=>act({page:"prev"});
$("next").onclick=()=>act({page:"next"});
$("power").onclick=()=>disp&&act({power:!disp.on});
$("sync").onclick=async()=>{
  $("sync").disabled=true;$("syncmsg").textContent="sincronizando…";
  try{await post("/api/v1/cloud/sync");setTimeout(async()=>{await tick();$("syncmsg").innerHTML='<span class="okc">feito</span>';$("sync").disabled=false},4000)}
  catch(e){$("syncmsg").innerHTML='<span class="err">'+e.message+"</span>";$("sync").disabled=false}
};
$("reboot").onclick=async()=>{
  const pw=$("pw_rb").value||$("pw").value;if(!pw){$("rbmsg").innerHTML='<span class="err">Digite a senha da estação.</span>';return}
  try{await post("/api/v1/system/reboot",{},{Authorization:auth(pw)});$("rbmsg").textContent="Reiniciando… a página volta em cerca de 15 s.";$("pw_rb").value=""}
  catch(e){$("rbmsg").innerHTML='<span class="err">'+(e.message.includes("401")?"Senha incorreta.":e.message)+"</span>"}
};
$("urlsave").onclick=async()=>{
  const u=$("c_url").value.trim(),pw=$("pw_url").value;
  if(!/^https:\/\//.test(u))return say("urlmsg","Use um endereço que comece com https://",1);
  if(!pw)return say("urlmsg","Digite a senha da estação.",1);
  try{
    const r=await fetch("/api/v1/cloud/url",{method:"POST",headers:{"Content-Type":"application/json",Authorization:auth(pw)},body:JSON.stringify({url:u})});
    const j=await r.json().catch(()=>({}));
    if(r.status===401)throw new Error("Senha incorreta.");
    if(!r.ok)throw new Error(j.detail||j.error||("HTTP "+r.status));
    $("pw_url").value="";delete $("c_url").dataset.edit;say("urlmsg","Endereço trocado; enviando para o novo painel…");setTimeout(tick,4000);
  }catch(e){say("urlmsg",e.message,1)}
};
$("c_url").oninput=()=>$("c_url").dataset.edit=1;
$("pwchg").onclick=async()=>{
  const cur=$("pw_cur").value,nw=$("pw_new").value;
  if(!cur)return say("pwmsg","Digite a senha atual.",1);
  if(nw.length<8||nw.length>63||/[^\x21-\x7e]/.test(nw))return say("pwmsg","A nova senha precisa ter de 8 a 63 caracteres, sem espaços nem acentos.",1);
  if(nw!==$("pw_new2").value)return say("pwmsg","As duas senhas novas não são iguais.",1);
  try{
    const r=await fetch("/api/v1/system/password",{method:"POST",headers:{"Content-Type":"application/json",Authorization:auth(cur)},body:JSON.stringify({new:nw})});
    const j=await r.json().catch(()=>({}));
    if(r.status===401)throw new Error("Senha atual incorreta.");
    if(!r.ok)throw new Error(j.detail||j.error||("HTTP "+r.status));
    ["pw_cur","pw_new","pw_new2"].forEach(id=>$(id).value="");
    say("pwmsg","Senha trocada. Use a nova a partir de agora.");
  }catch(e){say("pwmsg",e.message,1)}
};
// Contadores: relógio da estação (epoch do /status) acertado a cada leitura e contado aqui a cada segundo.
let devOff=null,ivl=300,rainAt=0,pageAt=Date.now()+5000,tests=null,pmode="auto";
const mmss=s=>s<=0?"agora":Math.floor(s/60)+":"+String(Math.floor(s%60)).padStart(2,"0");
function timers(){
  const pg=Math.max(0,Math.ceil((pageAt-Date.now())/1000));$("n_page").textContent="em "+pg+" s";
  if(devOff==null)return;
  const e=Date.now()/1000+devOff,ns=Math.ceil(e/ivl)*ivl-e,ttl=mmss(ns);
  $("n_send").textContent="em "+ttl+" (a cada "+ivl/60+" min)";$("osend").textContent="envio em "+ttl;
  for(const k of ["h72","d30"]){const t=tests&&tests[k],el=$("x_"+k);if(!t)continue;
    el.innerHTML=!t.end?"aguardando início":t.state==="sucesso"?'<b class="ok">passou</b>':(t.state==="falhou"?'<span class="err">com falha</span> · ':"")+"faltam "+dur(Math.max(0,t.end-e))}
  if(rainAt){const nr=rainAt+600-e;const t=nr>0?"em "+mmss(Math.max(nr,ns)):"no próximo envio ("+mmss(ns)+")";$("n_rain").textContent=t;$("rnext").textContent=" · próxima "+t}
}
setInterval(timers,1000);
const LV=p=>p>=60?["a","alta"]:p>=30?["m","média"]:["","baixa"];
function renderRain(r){
  const tiles=$("rt").children;
  for(let i=0;i<3;i++){const t=tiles[i],p=r.fresh&&r.p?r.p[i]:null,l=p==null?["",""]:LV(p);
    t.className=l[0];t.children[1].textContent=p==null?"–":p+"%";t.children[2].firstChild.style.width=(p||0)+"%";t.children[3].textContent=p==null?"–":l[1]+(r.m?" · "+r.m[i]:"")}
  $("rwhy").textContent=r.fresh?(r.reason+" · calculada há "+Math.round(r.age_s/60)+" min"):(r.at?"Previsão antiga (há "+Math.round(r.age_s/60)+" min): aguardando a nuvem.":"Aguardando a primeira previsão da nuvem (chega com o próximo envio).");
  const ms=r.fresh&&r.methods,rows=$("mt").tBodies[0].rows;$("mt").hidden=!ms;$("mthelp").hidden=!ms;
  if(ms)["m1","m2","m2t","m3","m4"].forEach((k,j)=>{const v=ms[k];for(let i=0;i<3;i++){const c=rows[j].cells[i+1];c.textContent=v?v[i]+"%":"–";c.className=r.m&&r.m[i]===k.toUpperCase()?"best":""}});
  const al=r.alert;$("ralert").hidden=!al.active;if(al.active)$("ralert").textContent="☂ Alerta: "+al.pct+"% de chance de chuva em até "+al.hours+" h";
  if(!$("rmsg").dataset.busy)$("rmsg").textContent=r.report_pending||r.no_rain_pending?"enviando aviso…":[r.last_report?"chuva avisada às "+hm(r.last_report):"",r.last_no_rain?"“não choveu hoje” às "+hm(r.last_no_rain):""].filter(Boolean).join(" · ");
}
$("rrep").onclick=async()=>{$("rrep").disabled=true;$("rmsg").dataset.busy=1;
  try{await post("/api/v1/rain/report");say("rmsg","Aviso registrado. Obrigado: isso entra no placar.")}catch(e){say("rmsg",e.message,1)}
  setTimeout(()=>{delete $("rmsg").dataset.busy;$("rrep").disabled=false},8000)};
$("rnone").onclick=async()=>{if(!confirm("Marcar que não choveu aqui hoje? As chuvas que o aeroporto registrou desde a meia-noite deixam de contar no placar e no treino."))return;
  $("rnone").disabled=true;$("rmsg").dataset.busy=1;
  try{await post("/api/v1/rain/report",{kind:"nao_choveu_hoje"});say("rmsg","Registrado: hoje não choveu aqui. A nuvem confere o dia de novo.")}catch(e){say("rmsg",e.message,1)}
  setTimeout(()=>{delete $("rmsg").dataset.busy;$("rnone").disabled=false},8000)};
$("a_test").onclick=()=>act({alert_test:true});
$("z_test").onclick=()=>act({buzzer_test:true});
async function tick(){
  pageAt=Date.now()+5000;
  try{
    const [now,st,d,rn]=await Promise.all(["/api/v1/now","/api/v1/status","/api/v1/display","/api/v1/rain"].map(u=>fetch(u).then(r=>r.json())));
    try{renderRain(rn)}catch(e){$("rwhy").textContent="Previsão indisponível nesta versão do firmware."}
    if(st.time.epoch)devOff=st.time.epoch-Date.now()/1000;
    if(st.cloud.interval_s)ivl=st.cloud.interval_s;
    tests=st.cloud.tests||null;
    rainAt=rn&&rn.at?rn.at:0;
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
    $("rst").textContent=st.device.reset_reason||"–";
    const bt=st.device.button;$("btn").textContent=bt&&bt.last?bt.last+(bt.at>1e9?" às "+hm(bt.at):""):"nenhum toque desde que ligou";
    $("boots").textContent=st.device.boots??"–";
    $("memmin").textContent=st.device.heap_min?Math.round(st.device.heap_min/1024)+" KB":"–";
    $("ota").textContent=st.device.ota?"ativa (porta 3232, senha da estação)":"indisponível";
    const c=st.cloud;
    if(!c.configured){$("c_ok").textContent="nuvem não configurada";$("sync").disabled=true}
    else{$("c_ok").innerHTML=c.last_error?'<span class="err">'+c.last_error+"</span>":(c.last_ok?hm(c.last_ok):"ainda não (envia a cada 10 min)");}
    $("c_q").textContent=c.queue;
    $("c_cfg").textContent=c.config_pending?"enviando…":(c.config_sent?"enviada às "+hm(c.config_sent):"–");
    if(c.url){$("cloud").href=c.url;$("cloud").hidden=false}
    if(!$("c_url").dataset.edit&&document.activeElement!==$("c_url"))$("c_url").value=c.url||"";
    renderDisplay(d);
    $("where").textContent=st.wifi.hostname+" · "+st.wifi.ip;
    $("oled").textContent=d.on?"display: "+(NAMES[d.pages[d.page]]||d.pages[d.page]):"display desligado";
    $("upd").textContent=st.time.synced?st.time.local.slice(11,16):"";
  }catch(e){
    $("oled").textContent="sem resposta da estação";$("upd").textContent="";
  }
}
loadCfg().catch(()=>say("savemsg","Não foi possível ler a configuração.",1));
tick();setInterval(tick,5000);
</script>
</body>
</html>
)HTML";
