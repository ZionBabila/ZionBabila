#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#14122b">
<title>UV Timer</title>
<style>
:root{--bg:#0e0d1f;--card:#1a1833;--card2:#221f40;--fg:#efeefb;--mut:#9c99c2;--line:#2f2b55;--acc:#9d5cff;--acc2:#c39bff;--on:#3ddc97;--off:#6b678f;--warn:#ffb547;--bad:#ff5c7a;--r:16px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:560px;margin:auto;padding:16px 16px 40px}
h2{font-size:15px;font-weight:650;margin:0;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:16px;margin-bottom:14px}
.row{display:flex;gap:10px;align-items:center}
.sp{justify-content:space-between}
.mut{color:var(--mut);font-size:14px}
.clock{font-size:46px;font-weight:700;letter-spacing:-.02em;font-variant-numeric:tabular-nums;line-height:1.1}
.badge{font-size:12px;font-weight:650;padding:4px 10px;border-radius:99px;background:var(--card2);color:var(--mut);white-space:nowrap}
.badge.ok{color:var(--on);background:#163b30}.badge.warn{color:var(--warn);background:#3d2e12}
button{font:inherit;border:0;border-radius:12px;padding:12px 14px;background:var(--card2);color:var(--fg);cursor:pointer;font-weight:600}
button:active{transform:scale(.98)}
.pri{background:var(--acc);color:#fff}
.ghost{background:transparent;color:var(--acc2);padding:8px 4px}
.danger{background:transparent;color:var(--bad)}
.grow{flex:1}
.seg{display:grid;grid-auto-flow:column;grid-auto-columns:1fr;background:var(--bg);border-radius:12px;padding:4px;gap:4px}
.seg button{background:transparent;color:var(--mut);padding:10px 6px}
.seg button.sel{background:var(--acc);color:#fff}
.status{display:flex;align-items:center;gap:14px}
.bulb{width:52px;height:52px;border-radius:50%;background:var(--card2);display:grid;place-items:center;font-size:22px;flex:none;transition:.3s}
.bulb.on{background:radial-gradient(circle,#c39bff 0,#7b2cff 70%);box-shadow:0 0 28px #9d5cffaa}
.big{font-size:22px;font-weight:700}
input,select{font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:11px 12px;width:100%}
input:focus,select:focus{outline:2px solid var(--acc);border-color:transparent}
input[type=range]{padding:0;border:0;background:none;accent-color:var(--acc);height:28px}
label.f{display:block;font-size:13px;color:var(--mut);margin:14px 0 6px}
.ev{display:flex;align-items:center;gap:12px;padding:12px 0;border-top:1px solid var(--line);cursor:pointer}
.ev:first-of-type{border-top:0}
.ev .t{font-size:19px;font-weight:700;font-variant-numeric:tabular-nums}
.ev.dis{opacity:.45}
.lvl{width:6px;align-self:stretch;border-radius:3px;background:var(--acc);flex:none}
.sw{position:relative;width:48px;height:28px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:var(--off);border-radius:99px;transition:.2s}
.sw span:before{content:"";position:absolute;width:22px;height:22px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.sw input:checked+span{background:var(--acc)}
.sw input:checked+span:before{transform:translateX(20px)}
.empty{text-align:center;padding:18px 0;color:var(--mut)}
.week{display:grid;grid-template-columns:34px 1fr;gap:6px 8px;align-items:center;font-size:12px;color:var(--mut)}
.bar{position:relative;height:16px;background:var(--bg);border-radius:5px;overflow:hidden}
.bar i{position:absolute;top:0;bottom:0;background:var(--acc);border-radius:3px}
.bar b{position:absolute;top:-2px;bottom:-2px;width:2px;background:#fff}
.today{color:var(--fg);font-weight:700}
.ticks{display:flex;justify-content:space-between;font-size:11px;color:var(--mut);grid-column:2}
.days{display:grid;grid-template-columns:repeat(7,1fr);gap:6px}
.days button{padding:11px 0;background:var(--bg);color:var(--mut);border:1px solid var(--line)}
.days button.sel{background:var(--acc);color:#fff;border-color:var(--acc)}
.chips{display:flex;gap:8px;margin-top:8px;flex-wrap:wrap}
.chips button{padding:6px 12px;font-size:13px;border-radius:99px}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.hint{font-size:13px;color:var(--mut);margin-top:6px}
.banner{border-radius:12px;padding:12px 14px;margin-top:12px;font-size:14px;background:#3d2e12;color:#ffd38a}
.banner.bad{background:#46182a;color:#ffb3c1}
details summary{list-style:none;cursor:pointer;display:flex;justify-content:space-between;align-items:center}
details summary::-webkit-details-marker{display:none}
details[open] summary .chev{transform:rotate(180deg)}
.chev{transition:.2s;color:var(--mut)}
.sheet{position:fixed;inset:0;background:#000a;display:none;align-items:flex-end;justify-content:center;z-index:9}
.sheet.open{display:flex}
.panel{background:var(--card);width:100%;max-width:560px;max-height:92vh;overflow:auto;border-radius:22px 22px 0 0;padding:18px 16px calc(18px + env(safe-area-inset-bottom))}
.panel h3{margin:0 0 4px;font-size:20px}
.actions{display:flex;gap:10px;margin-top:20px}
.uvw{display:flex;gap:12px;align-items:flex-start;background:#3a2a05;border:1px solid #b8860b;color:#ffe08a;border-radius:var(--r);padding:14px;margin-bottom:14px;font-size:14px}
.uvw .ico{flex:none;width:34px;height:34px;border-radius:8px;background:#ffcc00;color:#000;display:grid;place-items:center;font-size:20px;font-weight:800}
.uvw b{color:#fff3c4}
.uvon{margin-top:12px;border-radius:12px;padding:10px 12px;font-size:13px;font-weight:600;background:#4a1d6e;color:#e9d5ff}
#toast{position:fixed;left:50%;top:16px;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:10px 18px;border-radius:99px;font-weight:600;font-size:14px;opacity:0;transition:.25s;pointer-events:none;z-index:10}
#toast.show{opacity:1}
</style>
</head>
<body>
<main>

<!-- UV safety -->
<section class="uvw" role="alert">
  <div class="ico">⚠</div>
  <div><b>UV radiation warning – UV‑A LEDs, 365–370 nm.</b><br>
  Never look directly at the LEDs. Wear UV‑blocking safety glasses and protect exposed skin while they are on. Keep away from children and pets.</div>
</section>

<!-- Clock -->
<section class="card">
  <div class="row sp"><h2>Device clock</h2><span class="badge" id="clkBadge">—</span></div>
  <div class="clock" id="clock">--:--:--</div>
  <div class="mut" id="date">Clock not set</div>
  <div class="row" style="margin-top:14px">
    <button class="pri grow" onclick="syncPhone()">Sync with phone</button>
    <button class="grow" onclick="openTime()">Set manually</button>
  </div>
  <div class="banner" id="clkWarn" hidden>The clock is not set, so events won't run. Tap <b>Sync with phone</b>.</div>
</section>

<!-- Status / mode -->
<section class="card">
  <div class="status">
    <div class="bulb" id="bulb">💡</div>
    <div class="grow">
      <div class="big" id="stTxt">Off</div>
      <div class="mut" id="stSub">&nbsp;</div>
    </div>
  </div>
  <div class="seg" style="margin-top:14px">
    <button id="m0" onclick="setMode(0)">Schedule</button>
    <button id="m1" onclick="setMode(1)">Always on</button>
    <button id="m2" onclick="setMode(2)">Off</button>
  </div>
  <div id="manBox" hidden>
    <label class="f">Brightness <b id="manV" style="color:var(--fg)"></b></label>
    <input type="range" id="manL" min="1" max="100" oninput="manV.textContent=this.value+'%'" onchange="setMode(1)">
  </div>
  <div class="uvon" id="uvOn" hidden>⚠ UV is ON – wear eye protection, do not look at the LEDs.</div>
  <div class="banner bad" id="heatWarn" hidden></div>
</section>

<!-- Events -->
<section class="card">
  <div class="row sp" style="margin-bottom:6px"><h2>Events</h2><button class="ghost" onclick="openEv(-1)">+ Add event</button></div>
  <div id="evList"></div>
</section>

<!-- Week -->
<section class="card">
  <h2 style="margin-bottom:12px">Week at a glance</h2>
  <div class="week" id="week"></div>
</section>

<!-- Settings -->
<section class="card">
  <details>
    <summary><h2>Settings</h2><span class="chev">▾</span></summary>
    <label class="f">Max brightness limit <b id="maxV" style="color:var(--fg)"></b></label>
    <input type="range" id="maxL" min="1" max="100" oninput="maxV.textContent=this.value+'%'">
    <div class="hint">Caps every event and manual mode. Protects the LEDs.</div>
    <div class="two">
      <div><label class="f">Heat sensors</label>
        <select id="sens"><option value="0">None</option><option value="1">1 sensor</option><option value="2">2 sensors</option></select></div>
      <div><label class="f">Heatsink limit (°C)</label><input type="number" id="tlim" min="30" max="90" inputmode="numeric"></div>
    </div>
    <div class="two">
      <div><label class="f">PWM frequency (Hz)</label><input type="number" id="freq" min="100" max="19000" inputmode="numeric"></div>
      <div><label class="f">PWM signal</label>
        <select id="inv"><option value="0">Normal</option><option value="1">Inverted</option></select></div>
    </div>
    <button class="pri" style="width:100%;margin-top:16px" onclick="saveSettings()">Save settings</button>

    <div style="border-top:1px solid var(--line);margin:22px 0 4px"></div>
    <h2 style="margin-top:14px">Wi‑Fi network</h2>
    <label class="f">Network name</label><input id="ssid" maxlength="32" autocapitalize="off">
    <label class="f">New password (8+ characters, empty = no password)</label><input id="pass" type="password" maxlength="63">
    <button style="width:100%;margin-top:16px" onclick="saveWifi()">Save & restart</button>
    <div class="hint" id="rtcInfo"></div>
  </details>
</section>
</main>

<!-- Event editor -->
<div class="sheet" id="evSheet" onclick="if(event.target===this)closeEv()">
  <div class="panel">
    <h3 id="evTitle">New event</h3>
    <label class="f">Days</label>
    <div class="days" id="evDays"></div>
    <div class="chips">
      <button onclick="setDays(127)">Every day</button>
      <button onclick="setDays(62)">Mon–Fri</button>
      <button onclick="setDays(31)">Sun–Thu</button>
      <button onclick="setDays(0)">Clear</button>
    </div>
    <div class="two">
      <div><label class="f">Start time</label><input type="time" id="evStart" oninput="evHint()"></div>
      <div><label class="f">Duration</label>
        <div class="row" style="gap:6px"><input type="number" id="evH" min="0" max="24" inputmode="numeric" oninput="evHint()" aria-label="hours"><span class="mut">h</span>
        <input type="number" id="evM" min="0" max="59" inputmode="numeric" oninput="evHint()" aria-label="minutes"><span class="mut">m</span></div></div>
    </div>
    <div class="hint" id="evEnd"></div>
    <label class="f">Brightness <b id="evLV" style="color:var(--fg)"></b></label>
    <input type="range" id="evL" min="1" max="100" oninput="evLV.textContent=this.value+'%'">
    <div class="two">
      <div><label class="f">Turn on</label><div class="seg" id="evFi"><button data-v="0">Instant</button><button data-v="1">Fade in</button></div></div>
      <div><label class="f">Turn off</label><div class="seg" id="evFo"><button data-v="0">Instant</button><button data-v="1">Fade out</button></div></div>
    </div>
    <div id="fadeBox">
       <label class="f">Fade length (seconds)</label>
      <input type="number" id="evF" min="1" max="3600" inputmode="numeric" oninput="evHint()">
    </div>
    <div class="actions">
      <button class="danger" id="evDel" onclick="delEv()">Delete</button>
      <span class="grow"></span>
      <button onclick="closeEv()">Cancel</button>
      <button class="pri" onclick="saveEv()">Save</button>
    </div>
  </div>
</div>

<!-- Manual time -->
<div class="sheet" id="tSheet" onclick="if(event.target===this)tSheet.classList.remove('open')">
  <div class="panel">
    <h3>Set date & time</h3>
    <div class="two">
      <div><label class="f">Date</label><input type="date" id="tDate"></div>
      <div><label class="f">Time</label><input type="time" id="tTime" step="1"></div>
    </div>
    <div class="actions"><span class="grow"></span>
      <button onclick="tSheet.classList.remove('open')">Cancel</button>
      <button class="pri" onclick="saveTime()">Set clock</button></div>
  </div>
</div>
<div id="toast"></div>

<script>
const DS=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'],DL=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const MO=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
const $=id=>document.getElementById(id),p2=n=>String(n).padStart(2,'0');
const hm=m=>p2(Math.floor(m/60)%24)+':'+p2(m%60);
let S=null,EV=[],base=0,baseAt=0,edit=-1,D=null,loaded=false;

function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),2200)}
async function post(u,d){const r=await fetch(u,{method:'POST',body:new URLSearchParams(d)});if(!r.ok)throw new Error(await r.text());return r}
const nowSec=()=>base+Math.floor((Date.now()-baseAt)/1000);
const phoneLocal=()=>Math.floor(Date.now()/1000)-new Date().getTimezoneOffset()*60;
function dur(m){const h=Math.floor(m/60),r=m%60;return h&&r?`${h}h ${r}m`:h?`${h}h`:`${r}m`}
function fadeTxt(s){return s<60?`${s} s`:s%60?`${Math.floor(s/60)}m ${s%60}s`:`${s/60} min`}
function daysTxt(d){if(d==127)return'Every day';if(d==62)return'Mon–Fri';if(d==31)return'Sun–Thu';if(!d)return'No days';return DS.filter((_,i)=>d>>i&1).join(', ')}
function inTxt(s){const m=Math.round(s/60);return m<60?`${m} min`:m<1440?`${Math.floor(m/60)}h ${m%60}m`:`${Math.floor(m/1440)}d ${Math.floor(m%1440/60)}h`}

/* ---------- clock ---------- */
function tick(){
  if(!S)return;
  if(!S.timeSet){$('clock').textContent='--:--:--';$('date').textContent='Clock not set';return}
  const d=new Date(nowSec()*1000);
  $('clock').textContent=p2(d.getUTCHours())+':'+p2(d.getUTCMinutes())+':'+p2(d.getUTCSeconds());
  $('date').textContent=`${DL[d.getUTCDay()]}, ${d.getUTCDate()} ${MO[d.getUTCMonth()]} ${d.getUTCFullYear()}`;
}
async function setClock(t,msg){try{await post('/api/time',{t});toast(msg);await refresh()}catch(e){toast('Could not set clock')}}
function syncPhone(){setClock(phoneLocal(),'Clock synced with phone ✓')}
function openTime(){
  const d=new Date((S&&S.timeSet?nowSec():phoneLocal())*1000);
  $('tDate').value=`${d.getUTCFullYear()}-${p2(d.getUTCMonth()+1)}-${p2(d.getUTCDate())}`;
  $('tTime').value=`${p2(d.getUTCHours())}:${p2(d.getUTCMinutes())}:${p2(d.getUTCSeconds())}`;
  $('tSheet').classList.add('open');
}
function saveTime(){
  const [y,mo,da]=$('tDate').value.split('-').map(Number),[h,mi,se]=($('tTime').value+':0').split(':').map(Number);
  if(!y||isNaN(h)){toast('Enter date and time');return}
  $('tSheet').classList.remove('open');
  setClock(Date.UTC(y,mo-1,da,h,mi,se||0)/1000,'Clock set ✓');
}

/* ---------- status ---------- */
function activeEnd(now){
  const wd=new Date(now*1000).getUTCDay(),sod=now%86400;let end=null;
  EV.forEach(e=>{if(!e.on)return;for(let b=0;b<2;b++){if(!(e.d>>((wd+7-b)%7)&1))continue;
    const el=sod+b*86400-e.s*60;if(el>=0&&el<e.u*60){const t=now-el+e.u*60;if(end===null||t>end)end=t}}});
  return end;
}
function nextStart(now){
  const wd=new Date(now*1000).getUTCDay(),day0=now-now%86400;let best=null;
  EV.forEach(e=>{if(!e.on)return;for(let k=0;k<8;k++){if(!(e.d>>((wd+k)%7)&1))continue;
    const t=day0+k*86400+e.s*60;if(t>now&&(best===null||t<best))best=t}});
  return best;
}
function renderStatus(){
  const out=Math.round(S.output),on=S.output>0.05;
  $('bulb').className='bulb'+(on?' on':'');$('uvOn').hidden=!on;
  $('stTxt').textContent=on?`On · ${out<1?'<1':out}%`:'Off';
  let sub='';
  if(S.mode==1)sub='Always on (manual)';
  else if(S.mode==2)sub='Turned off (manual)';
  else if(!S.timeSet)sub='Waiting for clock';
  else{const now=nowSec(),end=activeEnd(now),nx=nextStart(now);
    if(end)sub=`Until ${hm(Math.floor(end%86400/60))}`;
    else if(nx){const d=new Date(nx*1000),k=Math.floor(nx/86400)-Math.floor(now/86400);
      sub=`Next: ${k==0?'today':k==1?'tomorrow':DS[d.getUTCDay()]} ${hm(Math.floor(nx%86400/60))} · in ${inTxt(nx-now)}`}
    else sub='No upcoming events'}
  if(S.sensors)sub+=' · 🌡 '+S.temps.map(t=>t===null?'--':t.toFixed(0)+'°C').join(' / ');
  $('stSub').textContent=sub;
  for(let i=0;i<3;i++)$('m'+i).classList.toggle('sel',S.mode==i);
  $('manBox').hidden=S.mode!=1;
  const hw=$('heatWarn');
  if(S.sensors&&S.fault){hw.hidden=false;hw.textContent=`Heat sensor not connected – brightness limited to ${S.tcap}%`}
  else if(S.sensors&&S.tcap<100){hw.hidden=false;hw.textContent=`Heatsink above ${S.tlimit}°C – brightness limited to ${S.tcap}%`}
  else hw.hidden=true;
  $('clkWarn').hidden=S.timeSet;
  const b=$('clkBadge');
  if(!S.timeSet){b.textContent='Not set';b.className='badge warn'}
  else{const diff=Math.abs(nowSec()-phoneLocal());
    if(diff>120){b.textContent='Differs from phone';b.className='badge warn'}
    else{b.textContent=S.rtc?'Set · battery backup':'Set';b.className='badge ok'}}
}
async function setMode(m){try{await post('/api/mode',{mode:m,level:$('manL').value});await refresh()}catch(e){toast('Error')}}

/* ---------- events ---------- */
function renderEvents(){
  const box=$('evList');
  if(!EV.length){box.innerHTML='<div class="empty">No events yet.<br>Tap <b>+ Add event</b> to create one.</div>';return}
  const order=EV.map((e,i)=>i).sort((a,b)=>EV[a].s-EV[b].s);
  box.innerHTML=order.map(i=>{const e=EV[i],end=e.s+e.u;
    return `<div class="ev${e.on?'':' dis'}" onclick="openEv(${i})">
      <div class="lvl" style="opacity:${.25+e.l/133}"></div>
      <div class="grow"><div class="t">${hm(e.s)} – ${hm(end)}${end>=1440?' <span class="mut">+1</span>':''}</div>
      <div class="mut">${daysTxt(e.d)} · ${e.l}% · ${dur(e.u)}</div>
      <div class="mut">${e.fi?'Fade in':'Instant on'} → ${e.fo?'fade out':'instant off'}${e.fi||e.fo?` (${fadeTxt(e.f)})`:''}</div></div>
      <label class="sw" onclick="event.stopPropagation()"><input type="checkbox" ${e.on?'checked':''} onchange="toggleEv(${i},this.checked)"><span></span></label>
    </div>`}).join('');
}
function renderWeek(){
  const segs=[[],[],[],[],[],[],[]];
  EV.forEach(e=>{if(!e.on)return;for(let d=0;d<7;d++){if(!(e.d>>d&1))continue;
    const a=e.s,b=e.s+e.u;segs[d].push([a,Math.min(b,1440),e.l]);if(b>1440)segs[(d+1)%7].push([0,b-1440,e.l])}});
  let today=-1,mark=0;
  if(S&&S.timeSet){const n=nowSec();today=new Date(n*1000).getUTCDay();mark=n%86400/864}
  let h='';
  for(let d=0;d<7;d++){
    h+=`<div class="${d==today?'today':''}">${DS[d]}</div><div class="bar">`+
      segs[d].map(([a,b,l])=>`<i style="left:${a/14.4}%;width:${Math.max((b-a)/14.4,.6)}%;opacity:${.3+l/143}"></i>`).join('')+
      (d==today?`<b style="left:${mark}%"></b>`:'')+'</div>'}
  h+='<div class="ticks"><span>0</span><span>6</span><span>12</span><span>18</span><span>24</span></div>';
  $('week').innerHTML=h;
}
async function pushEvents(msg){
  try{await post('/api/events',{e:EV.map(e=>[e.d,e.s,e.u,e.l,e.fi,e.fo,e.f,e.on].join(',')).join(';')});if(msg)toast(msg)}
  catch(e){toast('Could not save – check connection')}
  await refresh(true);
}
function toggleEv(i,v){EV[i].on=v?1:0;renderEvents();renderWeek();pushEvents(v?'Event enabled':'Event paused')}

function segSet(id,v){[...$(id).children].forEach(b=>b.classList.toggle('sel',b.dataset.v==v))}
['evFi','evFo'].forEach(id=>$(id).addEventListener('click',ev=>{const b=ev.target.closest('button');if(!b)return;
  D[id=='evFi'?'fi':'fo']=+b.dataset.v;segSet(id,b.dataset.v);evHint()}));
function renderDays(){$('evDays').innerHTML=DS.map((n,i)=>`<button class="${D.d>>i&1?'sel':''}" onclick="D.d^=${1<<i};renderDays()">${n[0]}${n[1]}</button>`).join('')}
function setDays(v){D.d=v;renderDays()}
function openEv(i){
  if(EV.length>=20&&i<0){toast('Maximum 20 events');return}
  edit=i;D=i<0?{d:127,s:18*60,u:120,l:100,fi:1,fo:1,f:10,on:1}:{...EV[i]};
  $('evTitle').textContent=i<0?'New event':'Edit event';$('evDel').hidden=i<0;
  renderDays();$('evStart').value=hm(D.s);$('evH').value=Math.floor(D.u/60);$('evM').value=D.u%60;
  $('evL').value=D.l;$('evLV').textContent=D.l+'%';$('evF').value=D.f;segSet('evFi',D.fi);segSet('evFo',D.fo);
  evHint();$('evSheet').classList.add('open');
}
function closeEv(){$('evSheet').classList.remove('open')}
function readDur(){return(+$('evH').value||0)*60+(+$('evM').value||0)}
function evHint(){
  const [h,m]=($('evStart').value||'0:0').split(':').map(Number),s=h*60+m,u=readDur();
  $('fadeBox').hidden=!(D.fi||D.fo);
  let t=u>0?`Ends at ${hm(s+u)}${s+u>=1440?' (next day)':''} · ${dur(u)}`:'Set a duration';
  const f=+$('evF').value||0,n=(D.fi?1:0)+(D.fo?1:0);
  if(n&&u>0&&f*n>u*60)t+=' · fades will be shortened to fit';
  $('evEnd').textContent=t;
}
async function saveEv(){
  const st=$('evStart').value,u=readDur(),f=+$('evF').value;
  if(!D.d){toast('Pick at least one day');return}
  if(!st){toast('Pick a start time');return}
  if(u<1||u>1440){toast('Duration must be 1 min – 24 h');return}
  if((D.fi||D.fo)&&(f<1||f>3600)){toast('Fade length: 1–3600 seconds');return}
  const [h,m]=st.split(':').map(Number);
  Object.assign(D,{s:h*60+m,u,l:+$('evL').value,f:f||D.f});
  if(edit<0)EV.push(D);else EV[edit]=D;
  closeEv();renderEvents();renderWeek();await pushEvents('Event saved ✓');
}
async function delEv(){if(!confirm('Delete this event?'))return;EV.splice(edit,1);closeEv();renderEvents();renderWeek();await pushEvents('Event deleted')}

/* ---------- settings ---------- */
async function saveSettings(){
  try{await post('/api/settings',{maxLevel:$('maxL').value,sensors:$('sens').value,tlimit:$('tlim').value,freq:$('freq').value,invert:$('inv').value});toast('Settings saved ✓');refresh()}
  catch(e){toast('Could not save')}
}
async function saveWifi(){
  const s=$('ssid').value.trim(),p=$('pass').value;
  if(!s){toast('Enter a network name');return}
  if(p&&p.length<8){toast('Password needs 8+ characters');return}
  if(!confirm(`Restart with network "${s}"${p?'':' (no password)'}?\nReconnect your phone to the new network afterwards.`))return;
  try{await post('/api/wifi',{ssid:s,pass:p});toast('Restarting… reconnect to '+s)}catch(e){toast(String(e.message||'Error'))}
}

/* ---------- loop ---------- */
async function refresh(evToo){
  let r;try{r=await (await fetch('/api/state',{cache:'no-store'})).json()}catch(e){return}
  S=r;base=r.now;baseAt=Date.now();
  if(!loaded||evToo){EV=r.events;renderEvents()}
  if(!loaded){loaded=true;
    $('manL').value=r.manualLevel;$('manV').textContent=r.manualLevel+'%';
    $('maxL').value=r.maxLevel;$('maxV').textContent=r.maxLevel+'%';
    $('sens').value=r.sensors;$('tlim').value=r.tlimit;$('freq').value=r.freq;$('inv').value=r.invert?1:0;$('ssid').value=r.ssid;
    $('rtcInfo').textContent=r.rtc?'Clock module (DS3231) detected – time is kept during power cuts.':'No clock module detected – after a power cut, open this page to set the time again.';
    if(!r.timeSet){syncPhone();return}
  }
  tick();renderStatus();renderWeek();
}
refresh();
setInterval(tick,250);
setInterval(()=>{if(!$('evSheet').classList.contains('open'))refresh()},3000);
</script>
</body>
</html>)HTML";
