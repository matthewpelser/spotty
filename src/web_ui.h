#pragma once
#include <Arduino.h>

// Single-page UI served at "/". Plain HTML/CSS/JS, no external assets
// (the controller may be used on a LAN without internet).
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Spotty</title>
<style>
:root{--bg:#15171a;--card:#1f2226;--line:#2e3238;--fg:#e8e8e8;--dim:#9aa0a6;--acc:#4fb286;--warn:#d9a23c;--bad:#d9534f}
@media (prefers-color-scheme: light){:root{--bg:#f2f2f0;--card:#fff;--line:#ddd;--fg:#1d1d1d;--dim:#666}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:4px 0 12px;display:flex;align-items:center;gap:10px}
h2{font-size:15px;margin:0 0 10px;color:var(--dim);font-weight:600;text-transform:uppercase;letter-spacing:.05em}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:16px}
.chips{display:flex;flex-wrap:wrap;gap:6px;font-size:13px}
.chip{border:1px solid var(--line);border-radius:999px;padding:2px 10px;color:var(--dim)}
.chip.ok{color:var(--acc);border-color:var(--acc)}.chip.warn{color:var(--warn);border-color:var(--warn)}
/* simulated 240x280 LCD: shows the exact framebuffer the device draws */
.device{display:flex;justify-content:center;margin:0 auto 16px}
.bezel{background:#08090a;padding:14px;border-radius:44px;box-shadow:0 6px 24px rgba(0,0,0,.35),inset 0 0 0 1px #2a2d31}
#screen{display:block;width:240px;height:280px;border-radius:32px;background:#0e1013;image-rendering:pixelated}
@media (min-width:560px){#screen{width:300px;height:350px;border-radius:40px}}
.screen-meta{text-align:center;color:var(--dim);font-size:12px;margin:-8px 0 14px}
.pad{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.pad button{min-height:52px}
button{background:var(--line);color:var(--fg);border:0;border-radius:8px;padding:8px 12px;font:inherit;cursor:pointer}
button:hover{filter:brightness(1.2)}button:active{transform:translateY(1px)}
button.preset{background:var(--acc);color:#fff;font-weight:600}
button.preset small{display:block;font-weight:400;font-size:12px;opacity:.9;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
button.primary{background:var(--acc);color:#fff}
.preset-edit{border-top:1px solid var(--line);padding:14px 0}.preset-edit:first-of-type{border-top:0;padding-top:0}
.preset-edit h3{margin:0 0 8px;font-size:15px}
label{display:block;color:var(--dim);font-size:13px;margin:8px 0 4px}
input[type=text],select{width:100%;padding:8px;border-radius:6px;border:1px solid var(--line);background:var(--bg);color:var(--fg);font:inherit}
.rooms{display:flex;flex-wrap:wrap;gap:6px 14px}.rooms label,.inline{display:inline-flex;align-items:center;gap:6px;color:var(--fg);margin:0;font-size:14px}
.vol{display:flex;align-items:center;gap:10px}.vol input[type=range]{flex:1}
.actions{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap}
.hint{color:var(--dim);font-size:13px}.warn-text{color:var(--warn);font-size:13px;margin:6px 0 0}button.preset.bad{background:var(--warn)}#toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#333;color:#fff;padding:8px 14px;border-radius:8px;opacity:0;transition:opacity .2s}
</style></head><body><main>
<h1>Spotty</h1>

<section class="card">
  <div class="device"><div class="bezel"><canvas id="screen" width="240" height="280" aria-label="Controller screen"></canvas></div></div>
  <div class="screen-meta">240 &times; 280 display preview</div>
  <div class="pad" id="pad"></div>
  <div class="pad" style="margin-top:8px">
    <button onclick="act('playpause')">Play / Pause</button>
    <button onclick="act('stop')">Stop</button>
    <button onclick="act('voldown')">Vol &minus;</button>
    <button onclick="act('volup')">Vol +</button>
    <button onclick="act('next')">Next</button>
    <button onclick="act('random')">Random fav</button>
    <button onclick="act('rescan');setTimeout(load,8000)" style="grid-column:span 2">Rescan Sonos</button>
  </div>
</section>

<section class="card"><h2>Status</h2><div class="chips" id="chips"></div></section>

<section class="card"><h2>Presets</h2>
  <p class="hint">The first ticked room leads the group. Choose from <b>Sonos Favourites</b> or, once connected below, your <b>Spotify</b> playlists and albums. TuneIn and Sonos Radio favourites can't be started by the controller.</p>
  <div id="presets"></div>
  <div class="actions"><button class="primary" onclick="save()">Save presets</button></div>
</section>

<section class="card"><h2>Music services</h2><div id="services"><span class="hint">Loading…</span></div></section>
</main><div id="toast"></div>
<script>
const $=s=>document.querySelector(s), esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
let rooms=[],favs=[],presets=[];
const j=(u,o)=>fetch(u,o).then(r=>r.json());
function toast(t){const e=$('#toast');e.textContent=t;e.style.opacity=1;clearTimeout(e._t);e._t=setTimeout(()=>e.style.opacity=0,1800)}
function act(type,n=0){fetch(`/api/action?type=${type}&n=${n}`,{method:'POST'})}
const cv=$('#screen'),cx=cv.getContext('2d'),img=cx.createImageData(240,280);
let fBusy=false,fAgain=false,shownV=-1;
async function frame(v){ // fetch the device framebuffer (little-endian RGB565) and paint it
  if(v!==undefined&&v===shownV)return;
  if(fBusy){fAgain=true;return} fBusy=true;
  const ctl=new AbortController(),t=setTimeout(()=>ctl.abort(),5000); // never wait forever
  try{const px=new Uint16Array(await (await fetch('/api/screen',{cache:'no-store',signal:ctl.signal})).arrayBuffer()),d=img.data;
    if(px.length===240*280){
      for(let i=0;i<px.length;i++){const q=px[i],r=q>>11,g=(q>>5)&63,b=q&31,o=i*4;
        d[o]=(r<<3)|(r>>2);d[o+1]=(g<<2)|(g>>4);d[o+2]=(b<<3)|(b>>2);d[o+3]=255}
      cx.putImageData(img,0,0); if(v!==undefined)shownV=v;
    }
  }catch(e){}
  finally{clearTimeout(t);fBusy=false}
  if(fAgain){fAgain=false;frame()}
}
function pad(){$('#pad').innerHTML=presets.map((p,i)=>`<button class="preset${p.favUnsupported?' bad':''}" onclick="act('preset',${i})" title="${p.favUnsupported?esc(p.favUnsupported)+' favourite: not playable':''}">${i+1}${p.favUnsupported?' ⚠':''}<small>${esc(p.name)}</small></button>`).join('')}
const whyText=r=>`The controller can't start this (${r}). Choose something else, e.g. a Spotify playlist.`;
function favWarn(sel){const f=favs.find(f=>f.uri===sel.value),w=sel.nextElementSibling;w.hidden=!(f&&f.unsupported);if(f&&f.unsupported)w.textContent=whyText(f.unsupported)}
function favOptions(p){
  const opt=f=>`<option value="${esc(f.uri)}" ${f.uri===p.favUri?'selected':''} ${f.unsupported&&f.uri!==p.favUri?'disabled':''}>${esc(f.title)}${f.unsupported?` — can't play (${esc(f.unsupported)})`:''}</option>`;
  let h='<option value="">— choose —</option>';
  if(p.favUri&&!favs.some(f=>f.uri===p.favUri)) h+=`<option value="${esc(p.favUri)}" selected>${esc(p.favTitle||'(saved choice)')}</option>`;
  for(const g of [...new Set(favs.map(f=>f.group))]) h+=`<optgroup label="${esc(g)}">${favs.filter(f=>f.group===g).map(opt).join('')}</optgroup>`;
  return h;
}

function renderPresets(){
  pad();
  $('#presets').innerHTML=presets.map((p,i)=>{
    const known=new Set(rooms.map(r=>r.uuid));
    const rs=rooms.map(r=>`<label><input type="checkbox" data-room="${r.uuid}" ${p.rooms.includes(r.uuid)?'checked':''}> ${esc(r.name)}</label>`)
      .concat(p.rooms.filter(u=>!known.has(u)).map(u=>`<label><input type="checkbox" data-room="${u}" checked> <i>offline room</i></label>`)).join('');
    const fo=favOptions(p);
    const setVol=p.volume>=0, v=setVol?p.volume:30;
    return `<div class="preset-edit" data-i="${i}">
      <h3>Button ${i+1}</h3>
      <label>Name</label><input type="text" data-k="name" maxlength="24" value="${esc(p.name)}">
      <label>Rooms</label><div class="rooms">${rs||'<span class="hint">No rooms found yet</span>'}</div>
      <label>Play</label><select data-k="fav" onchange="favWarn(this)">${fo}</select>
      <p class="warn-text" ${p.favUnsupported?'':'hidden'}>${p.favUnsupported?esc(whyText(p.favUnsupported)):''}</p>
      <label>Volume</label>
      <div class="vol"><label class="inline"><input type="checkbox" data-k="setvol" ${setVol?'checked':''}> Set</label>
        <input type="range" min="0" max="100" value="${v}" data-k="vol" oninput="this.nextElementSibling.textContent=this.value">
        <span>${v}</span></div>
      <label class="inline" style="margin-top:8px"><input type="checkbox" data-k="shuffle" ${p.shuffle?'checked':''}> Shuffle (playlists/albums)</label>
      <div class="actions"><button onclick="save(${i})">Save &amp; test</button></div>
    </div>`}).join('');
}

function collect(){
  return [...document.querySelectorAll('.preset-edit')].map(el=>{
    const q=k=>el.querySelector(`[data-k=${k}]`), sel=q('fav');
    return {name:q('name').value.trim(),
      rooms:[...el.querySelectorAll('[data-room]')].filter(c=>c.checked).map(c=>c.dataset.room),
      favUri:sel.value, favTitle:sel.selectedOptions[0]?.textContent||'',
      volume:q('setvol').checked?+q('vol').value:-1, shuffle:q('shuffle').checked};
  });
}
async function save(test){
  const body=collect();
  const r=await fetch('/api/presets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  if(!r.ok){toast('Save failed');return}
  presets=await j('/api/presets'); pad(); toast('Saved');
  if(test!==undefined) act('preset',test);
}
async function state(){
  try{const s=await j('/api/state');
    const ph={ready:['ok','Sonos ready'],discovering:['warn','Finding Sonos…'],'no-sonos':['warn','No Sonos found'],starting:['','Starting']}[s.phase]||['',s.phase];
    $('#chips').innerHTML=[`<span class="chip ok">WiFi ${esc(s.ssid)} (${s.rssi} dBm)</span>`,`<span class="chip">${s.ip}</span>`,
      `<span class="chip ${ph[0]}">${ph[1]}</span>`,`<span class="chip">${s.rooms} rooms</span>`,`<span class="chip">${s.favourites} favourites</span>`,
      `<span class="chip">Group: ${s.group.length?esc(s.group.join(', ')):'none'}</span>`].join('');
    if(s.phase==='ready'&&!rooms.length) load();
    if(s.frame!==shownV) frame(s.frame); // missed a live notice: catch up
  }catch(e){$('#chips').innerHTML='<span class="chip warn">Controller offline</span>'}
}
const post=(u,data)=>fetch(u,{method:'POST',body:new URLSearchParams(data||{})}).then(r=>r.json());
let svcKey='',svcItems=-1,editClient=false;
function ago(sec){return sec<0?'':sec<90?'just now':sec<5400?Math.round(sec/60)+' min ago':Math.round(sec/3600)+' h ago'}
function renderServices(sp){
  const el=$('#services');
  const head=`<h3 style="margin:0 0 6px;font-size:15px">Spotify</h3>`;
  if(!sp.clientId||editClient){
    el.innerHTML=head+`<p class="hint">Optional. Lets you pick from your Spotify playlists, saved albums and new releases. Playback still goes through Sonos.</p>
      <ol class="hint" style="padding-left:18px;margin:6px 0">
        <li>Open the <a href="https://developer.spotify.com/dashboard" target="_blank" rel="noopener">Spotify developer dashboard</a> and choose <b>Create app</b> (any name; tick <b>Web API</b>).</li>
        <li>Add this <b>Redirect URI</b>: <code>${esc(sp.redirectUri)}</code> <button onclick="navigator.clipboard.writeText('${esc(sp.redirectUri)}');toast('Copied')" style="padding:2px 8px">Copy</button></li>
        <li>Paste the app's <b>Client ID</b> here.</li></ol>
      <label>Client ID</label><input type="text" id="sp-client" maxlength="32" placeholder="32 characters" value="${esc(sp.clientId||'')}">
      <div class="actions"><button class="primary" onclick="spClient()">Save</button>${sp.clientId?'<button onclick="editClient=false;svcKey=\'\';services()">Cancel</button>':''}</div>`;
    return;
  }
  if(sp.busy){el.innerHTML=head+'<p class="hint">Signing in and loading your lists…</p>';return}
  if(!sp.connected){
    el.innerHTML=head+`${sp.error?`<p class="warn-text">${esc(sp.error)}</p>`:''}
      <p class="hint">Step 1: sign in to Spotify in a new tab. After you approve, the tab goes to a page that <b>won't load</b> (that's expected).</p>
      <div class="actions"><button class="primary" onclick="spConnect()">Connect Spotify</button><button onclick="editClient=true;svcKey='';services()">Change client ID</button></div>
      <p class="hint" style="margin-top:12px">Step 2: copy the whole address from that tab's address bar (starts with <code>http://127.0.0.1:8888/callback?code=</code>) and paste it here.</p>
      <input type="text" id="sp-url" placeholder="http://127.0.0.1:8888/callback?code=…">
      <div class="actions"><button class="primary" onclick="spCallback()">Finish sign-in</button></div>`;
    return;
  }
  el.innerHTML=head+`${sp.error?`<p class="warn-text">${esc(sp.error)}</p>`:''}
    <p>Connected${sp.user?` as <b>${esc(sp.user)}</b>`:''} · ${sp.items} playlists &amp; albums${sp.updatedSecondsAgo>=0?` · updated ${ago(sp.updatedSecondsAgo)}`:''}</p>
    <p class="hint">Spotify's own lists (e.g. New Music Friday, Discover Weekly) appear here once you follow them in Spotify.</p>
    <div class="actions"><button onclick="act('sync');toast('Refreshing…')">Refresh lists</button><button onclick="spDisconnect()">Disconnect</button></div>`;
}
async function services(){
  try{const s=await j('/api/services'),sp=s.spotify,k=JSON.stringify({...sp,updatedSecondsAgo:Math.floor(sp.updatedSecondsAgo/60)});
    if(k!==svcKey){svcKey=k;renderServices(sp)}
    if(sp.items!==svcItems){const first=svcItems<0;svcItems=sp.items;if(!first)load()}
  }catch(e){}
}
async function spClient(){const r=await post('/api/services/spotify/client',{clientId:$('#sp-client').value.trim()});if(!r.ok)return toast(r.error);editClient=false;svcKey='';services()}
async function spConnect(){const r=await post('/api/services/spotify/connect');if(!r.ok)return toast(r.error);window.open(r.url,'_blank')}
async function spCallback(){const r=await post('/api/services/spotify/callback',{url:$('#sp-url').value.trim()});if(!r.ok)return toast(r.error);toast('Signing in…');svcKey='';setTimeout(services,800)}
async function spDisconnect(){if(!confirm('Disconnect Spotify from Spotty?'))return;await post('/api/services/spotify/disconnect');svcKey='';services();load()}
async function load(){[rooms,favs,presets]=await Promise.all([j('/api/rooms'),j('/api/favourites'),j('/api/presets')]);renderPresets()}
let es;
function connectEvents(){ // live frame notices; reconnect if the board restarts
  es=new EventSource('/events');
  es.addEventListener('frame',e=>{try{frame(JSON.parse(e.data).v)}catch(_){frame()}});
  es.onopen=()=>frame();
  es.onerror=()=>{if(es.readyState===EventSource.CLOSED)setTimeout(connectEvents,3000)};
}
connectEvents();
frame();load();state();services();setInterval(()=>{state();services()},5000);
</script></body></html>)HTML";
