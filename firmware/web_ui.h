#pragma once

const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Irrigatore</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f172a;color:#e2e8f0;padding:12px;max-width:420px;margin:auto}
h1{text-align:center;font-size:1.2rem;margin-bottom:12px;color:#38bdf8;padding:8px 0}
h1 small{font-size:.7rem;display:block;color:#475569;margin-top:2px}
.upd-banner{background:#422006;border:1px solid #92400e;color:#fde68a;border-radius:8px;padding:10px 12px;margin-bottom:10px;font-size:.82rem;display:none}
.upd-banner b{color:#fbbf24}
.upd-banner .btn-upd{display:inline-block;margin-top:8px;padding:7px 14px;background:#f59e0b;color:#0f172a;border:none;border-radius:6px;font-size:.82rem;font-weight:700;cursor:pointer;width:100%;text-align:center}
.upd-banner .btn-upd:hover{background:#fbbf24}
.upd-banner .btn-upd:disabled{background:#92400e;color:#fde68a;cursor:not-allowed}
.card{background:#1e293b;border-radius:10px;padding:12px;margin-bottom:10px;border:1px solid #2d3f55}
.card h2{font-size:.8rem;color:#64748b;margin-bottom:10px;text-transform:uppercase;letter-spacing:.06em}
label{display:block;font-size:.78rem;color:#94a3b8;margin-bottom:4px;margin-top:8px}
label:first-of-type{margin-top:0}
input[type=number],input[type=time]{width:100%;padding:7px 10px;background:#0f172a;border:1px solid #334155;border-radius:6px;color:#e2e8f0;font-size:.9rem}
input:focus{outline:none;border-color:#38bdf8}
.row{display:flex;gap:8px}
.row>*{flex:1}
.tog{display:flex;align-items:center;justify-content:space-between;padding:4px 0 8px}
.tog span{font-size:.85rem}
.sw{position:relative;width:42px;height:24px;flex-shrink:0}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:#334155;border-radius:24px;cursor:pointer;transition:.25s}
.sl:before{content:'';position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.25s}
input:checked+.sl{background:#38bdf8}
input:checked+.sl:before{transform:translateX(18px)}
.btn{display:block;width:100%;padding:9px;border:none;border-radius:7px;font-size:.9rem;cursor:pointer;font-weight:600;transition:.15s;margin-top:8px}
.bp{background:#38bdf8;color:#0f172a}.bp:hover{background:#7dd3fc}
.bw{background:#f59e0b;color:#0f172a}.bw:hover{background:#fbbf24}
.bs{background:#2d3f55;color:#94a3b8}.bs:hover{background:#334155}
.msg{text-align:center;padding:6px;border-radius:6px;font-size:.78rem;margin-top:8px;display:none}
.msg.ok{background:#064e3b;color:#6ee7b7;display:block}
.msg.err{background:#7f1d1d;color:#fca5a5;display:block}
#ota-dl-prog{display:none;margin-top:8px}
#op{display:none;margin-top:8px}
progress{width:100%;height:8px;border-radius:8px;overflow:hidden}
progress::-webkit-progress-bar{background:#1e293b}
progress::-webkit-progress-value{background:#38bdf8}
#ota-dl-pct,#opct{text-align:center;font-size:.75rem;color:#475569;margin-top:4px}
.ver{text-align:center;font-size:.7rem;color:#2d3f55;margin-top:10px;padding-bottom:4px}
</style>
</head>
<body>
<h1>&#127807; Irrigatore<small>v%VERSION% &mdash; irriga.local</small></h1>

<!-- Banner aggiornamento: appare via JS quando disponibile -->
<div class="upd-banner" id="upd-banner">
  <span id="upd-text"></span>
  <button class="btn-upd" id="upd-btn" onclick="installFromGitHub()">&#8593; Scarica e installa aggiornamento</button>
  <div id="ota-dl-prog">
    <progress id="ota-dl-bar" value="0" max="100"></progress>
    <div id="ota-dl-pct">Download: 0%</div>
  </div>
  <div id="ota-dl-msg" class="msg"></div>
</div>

<div class="card">
<h2>&#9201; Programmazione</h2>
<div class="tog">
  <span>Irrigazione automatica</span>
  <label class="sw"><input type="checkbox" id="ie" %IRRIG_EN%><span class="sl"></span></label>
</div>
<div class="row">
  <div><label>Orario</label><input type="time" id="it" value="%IRRIG_TIME%"></div>
  <div><label>Durata (s)</label><input type="number" id="id" min="1" max="3600" value="%IRRIG_DUR%"></div>
</div>
<button class="btn bp" onclick="saveIrrig()">Salva</button>
<div id="is" class="msg"></div>
</div>

<div class="card">
<h2>&#9654; Manuale</h2>
<div class="row" style="align-items:flex-end">
  <div><label>Durata (s)</label><input type="number" id="md" min="1" max="3600" value="30"></div>
  <div style="display:flex;flex-direction:column;gap:4px">
    <button class="btn bw" style="margin-top:18px" onclick="startM()">Avvia</button>
    <button class="btn bs" style="margin-top:4px" onclick="stopP()">Stop</button>
  </div>
</div>
<div id="ps" class="msg"></div>
</div>

<div class="card">
<h2>&#8593; Firmware OTA (manuale)</h2>
<input type="file" id="of" accept=".bin" style="color:#64748b;font-size:.78rem;width:100%">
<button class="btn bp" onclick="doOTA()">Carica file .bin</button>
<div id="op"><progress id="ob" value="0" max="100"></progress><div id="opct">0%</div></div>
<div id="os" class="msg"></div>
</div>

<p class="ver">Developed By Furios121 | Irrigatore Automatico | %VERSION%</p>

<script>
// URL del bin remoto (popolato da checkUpdate)
var remoteBinUrl = '';

function msg(id,t,ok){
  var e=document.getElementById(id);
  e.textContent=t;e.className='msg '+(ok?'ok':'err');
  if(ok)setTimeout(function(){e.style.display='none'},4000);
}

function saveIrrig(){
  var t=document.getElementById('it').value,d=document.getElementById('id').value,e=document.getElementById('ie').checked?1:0;
  if(!t||!d){msg('is','Compila i campi',false);return;}
  fetch('/save-irrig',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'time='+t+'&dur='+d+'&en='+e})
  .then(function(r){return r.text();}).then(function(r){msg('is',r==='OK'?'Salvato!':'Err: '+r,r==='OK');}).catch(function(){msg('is','Errore rete',false);});
}

function startM(){
  fetch('/pump-start?dur='+document.getElementById('md').value)
  .then(function(r){return r.text();}).then(function(r){msg('ps',r==='OK'?'Avviata!':'Err: '+r,r==='OK');}).catch(function(){msg('ps','Errore rete',false);});
}

function stopP(){
  fetch('/pump-stop').then(function(){msg('ps','Fermata',true);}).catch(function(){msg('ps','Errore rete',false);});
}

// OTA manuale da file locale
function doOTA(){
  var f=document.getElementById('of').files[0];
  if(!f){msg('os','Seleziona .bin',false);return;}
  uploadBin(f,'os','op','ob','opct');
}

// OTA automatico da GitHub — scarica il .bin tramite l'ESP (proxy /ota-github)
function installFromGitHub(){
  if(!remoteBinUrl){msg('ota-dl-msg','URL non disponibile',false);return;}
  var btn=document.getElementById('upd-btn');
  btn.disabled=true;
  btn.textContent='Installazione in corso...';
  document.getElementById('ota-dl-prog').style.display='block';
  // Chiede all'ESP di scaricare e flashare il .bin da GitHub
  fetch('/ota-github',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'url='+encodeURIComponent(remoteBinUrl)
  }).then(function(r){return r.text();}).then(function(r){
    if(r==='OK'){
      msg('ota-dl-msg','Aggiornamento completato! Riavvio in corso...',true);
      document.getElementById('ota-dl-bar').value=100;
      document.getElementById('ota-dl-pct').textContent='100%';
    } else {
      msg('ota-dl-msg','Errore: '+r,false);
      btn.disabled=false;btn.textContent='&#8593; Scarica e installa aggiornamento';
    }
  }).catch(function(){
    msg('ota-dl-msg','Errore di rete',false);
    btn.disabled=false;btn.textContent='&#8593; Scarica e installa aggiornamento';
  });
  // Simula progress bar durante il flash (non abbiamo byte count lato server)
  var p=0;
  var iv=setInterval(function(){
    if(p<90){p+=2;document.getElementById('ota-dl-bar').value=p;document.getElementById('ota-dl-pct').textContent='Flash: '+p+'%';}
    else{clearInterval(iv);}
  },400);
}

function uploadBin(file,msgId,progId,barId,pctId){
  var fd=new FormData();fd.append('firmware',file);
  var x=new XMLHttpRequest();x.open('POST','/update',true);
  document.getElementById(progId).style.display='block';
  x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);document.getElementById(barId).value=p;document.getElementById(pctId).textContent=p+'%';}};
  x.onload=function(){msg(msgId,x.status===200?'Completato! Riavvio...':'Errore: '+x.responseText,x.status===200);};
  x.onerror=function(){msg(msgId,'Errore rete',false);};
  x.send(fd);
}

function checkUpdate(){
  fetch('/update-info').then(function(r){return r.json();}).then(function(d){
    var b=document.getElementById('upd-banner');
    if(d.available){
      remoteBinUrl=d.bin_url||'';
      document.getElementById('upd-text').innerHTML='&#8593; Aggiornamento disponibile: <b>v'+d.version+'</b>'+(d.notes?' &mdash; '+d.notes:'');
      b.style.display='block';
      // Nascondi tasto se non c'è bin_url
      document.getElementById('upd-btn').style.display=remoteBinUrl?'block':'none';
    } else {
      b.style.display='none';
      remoteBinUrl='';
    }
  }).catch(function(){});
}
checkUpdate();
setInterval(checkUpdate,60000);
</script>
</body>
</html>
)rawhtml";
