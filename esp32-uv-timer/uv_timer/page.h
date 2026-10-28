#pragma once
#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="he" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>שעון UV</title>
<style>
:root{--bg:#0f1020;--card:#1a1b33;--fg:#e8e8f4;--mut:#9a9ab8;--acc:#9b5cff;--on:#3ddc97;--off:#ff5c7a;--line:#2c2d4d}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Arial,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:680px;margin:auto;padding:16px}
h1{font-size:22px;margin:4px 0 12px}
h2{font-size:17px;margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:14px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.big{font-size:28px;font-weight:700;font-variant-numeric:tabular-nums}
.mut{color:var(--mut);font-size:13px}
.pill{display:inline-block;padding:3px 10px;border-radius:99px;font-size:13px;font-weight:600}
.pill.on{background:#1f4d3c;color:var(--on)}.pill.off{background:#4d1f2b;color:var(--off)}
button{background:var(--line);color:var(--fg);border:0;border-radius:10px;padding:10px 14px;font-size:15px;cursor:pointer}
button.pri{background:var(--acc);color:#fff}
button.sel{outline:2px solid var(--acc);background:#2f2257}
button.del{background:transparent;color:var(--off);padding:6px 8px}
.seg{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;width:100%}
input,select{background:#11122a;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:8px;font-size:15px}
input[type=range]{width:100%;padding:0}
input[type=time]{min-width:110px}
.slot{border:1px solid var(--line);border-radius:12px;padding:10px;margin-bottom:10px}
.days{display:grid;grid-template-columns:repeat(7,1fr);gap:4px;margin:8px 0}
.days label{text-align:center;border:1px solid var(--line);border-radius:8px;padding:6px 0;font-size:14px;cursor:pointer;user-select:none}
.days input{display:none}
.days label.ck{background:var(--acc);border-color:var(--acc);color:#fff}
label.f{display:flex;flex-direction:column;gap:4px;font-size:13px;color:var(--mut);flex:1;min-width:120px}
.warn{background:#3a2a10;border:1px solid #7a5a1a;color:#ffd38a;border-radius:10px;padding:10px;font-size:14px}
#toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--acc);color:#fff;padding:10px 18px;border-radius:10px;display:none}
</style>
</head>
<body>
<main>
<h1>🟣 שעון נורת UV</h1>

<div class="card">
  <div class="row" style="justify-content:space-between">
    <div><div class="big" id="clock">--:--:--</div><div class="mut" id="date"></div></div>
    <div style="text-align:left"><span class="pill off" id="state">כבוי</span><div class="mut" id="outp"></div></div>
  </div>
  <div class="warn" id="timeWarn" style="display:none;margin-top:10px">השעון לא מכוון – במצב אוטומטי הנורה תישאר כבויה.
    <button class="pri" onclick="syncTime()" style="margin-top:8px">כוון שעה מהטלפון</button></div>
</div>

<div class="card">
  <h2>מצב פעולה</h2>
  <div class="seg">
    <button id="m0" onclick="setMode(0)">אוטומטי</button>
    <button id="m1" onclick="setMode(1)">דלוק ידני</button>
    <button id="m2" onclick="setMode(2)">כבוי</button>
  </div>
  <div id="manualBox" style="margin-top:12px;display:none">
    <div class="row" style="justify-content:space-between"><span>עוצמה ידנית</span><b id="mlv">100%</b></div>
    <input type="range" min="0" max="100" id="mlevel" oninput="mlv.textContent=this.value+'%'" onchange="setMode(1)">
  </div>
</div>

<div class="card">
  <h2>תוכנית שבועית</h2>
  <div id="slots"></div>
  <div class="row">
    <button onclick="addSlot()">+ הוסף פרק זמן</button>
    <button class="pri" onclick="saveSlots()">שמור תוכנית</button>
  </div>
  <p class="mut">פרק זמן שבו שעת הסיום מוקדמת משעת ההתחלה (למשל 22:00–06:00) ממשיך עד למחרת בבוקר. אם יש חפיפה – נבחרת העוצמה הגבוהה.</p>
</div>

<div class="card">
  <h2>הגדרות</h2>
  <div class="row">
    <label class="f">תדר PWM (Hz)<input type="number" id="freq" min="100" max="40000"></label>
    <label class="f">עוצמה מקסימלית %<input type="number" id="maxLevel" min="0" max="100"></label>
  </div>
  <div class="row" style="margin-top:8px">
    <label class="f">היפוך PWM<select id="invert"><option value="0">רגיל</option><option value="1">הפוך</option></select></label>
    <label class="f">אזור זמן (POSIX)<input id="tz" dir="ltr"></label>
  </div>
  <div class="row" style="margin-top:10px">
    <button class="pri" onclick="saveSettings()">שמור הגדרות</button>
    <button onclick="syncTime()">כוון שעה מהטלפון</button>
  </div>
</div>

<div class="card">
  <h2>Wi‑Fi</h2>
  <div class="mut" id="net"></div>
  <div class="row" style="margin-top:8px">
    <label class="f">שם רשת<input id="ssid" dir="ltr"></label>
    <label class="f">סיסמה<input id="pass" type="password" dir="ltr"></label>
  </div>
  <div class="row" style="margin-top:10px"><button class="pri" onclick="saveWifi()">שמור והפעל מחדש</button></div>
</div>
</main>
<div id="toast"></div>

<script>
const DN=['א','ב','ג','ד','ה','ו','ש'];
const DF=['ראשון','שני','שלישי','רביעי','חמישי','שישי','שבת'];
let S={},slots=[],loaded=false,dirty=false;
const $=id=>document.getElementById(id);
const pad=n=>String(n).padStart(2,'0');
const m2t=m=>pad(Math.floor(m/60))+':'+pad(m%60);
const t2m=t=>{const[a,b]=t.split(':');return(+a)*60+(+b)};
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2000)}
async function post(url,data){const r=await fetch(url,{method:'POST',body:new URLSearchParams(data)});if(!r.ok)throw 0;return r.text()}

function renderSlots(){
  const box=$('slots');box.innerHTML='';
  if(!slots.length)box.innerHTML='<p class="mut">אין פרקי זמן. הוסף אחד.</p>';
  slots.forEach((s,i)=>{
    const d=document.createElement('div');d.className='slot';
    let days='';for(let k=0;k<7;k++){const c=s.d&(1<<k);
      days+=`<label class="${c?'ck':''}" title="${DF[k]}"><input type="checkbox" ${c?'checked':''} onchange="tg(${i},${k},this)">${DN[k]}</label>`}
    d.innerHTML=`<div class="row" style="justify-content:space-between">
      <label class="row" style="gap:6px"><input type="checkbox" ${s.on?'checked':''} onchange="slots[${i}].on=this.checked?1:0;dirty=true"> פעיל</label>
      <span class="row" style="gap:4px"><button onclick="allDays(${i})">כל הימים</button><button class="del" onclick="delSlot(${i})">🗑 מחק</button></span></div>
      <div class="days">${days}</div>
      <div class="row">
        <label class="f">התחלה<input type="time" value="${m2t(s.s)}" onchange="slots[${i}].s=t2m(this.value);dirty=true"></label>
        <label class="f">סיום<input type="time" value="${m2t(s.e)}" onchange="slots[${i}].e=t2m(this.value);dirty=true"></label>
      </div>
      <div class="row" style="justify-content:space-between;margin-top:8px"><span class="mut">עוצמה</span><b id="lv${i}">${s.l}%</b></div>
      <input type="range" min="1" max="100" value="${s.l}" oninput="slots[${i}].l=+this.value;$('lv${i}').textContent=this.value+'%';dirty=true">`;
    box.appendChild(d);
  });
}
function tg(i,k,el){slots[i].d^=(1<<k);el.parentNode.classList.toggle('ck',el.checked);dirty=true}
function allDays(i){slots[i].d=127;dirty=true;renderSlots()}
function addSlot(){slots.push({d:127,s:18*60,e:22*60,l:100,on:1});dirty=true;renderSlots()}
function delSlot(i){slots.splice(i,1);dirty=true;renderSlots()}
async function saveSlots(){
  try{await post('/api/slots',{s:slots.map(s=>[s.d,s.s,s.e,s.l,s.on].join(',')).join(';')});dirty=false;toast('התוכנית נשמרה ✔')}catch(e){toast('שגיאה בשמירה')}
}
async function setMode(m){try{await post('/api/mode',{mode:m,level:$('mlevel').value});refresh()}catch(e){toast('שגיאה')}}
async function syncTime(){try{await post('/api/time',{epoch:Math.floor(Date.now()/1000)});toast('השעה כוונה ✔');refresh()}catch(e){toast('שגיאה')}}
async function saveSettings(){
  try{await post('/api/settings',{freq:$('freq').value,invert:$('invert').value,maxLevel:$('maxLevel').value,tz:$('tz').value});toast('נשמר ✔')}catch(e){toast('שגיאה')}
}
async function saveWifi(){
  if(!confirm('הבקר יופעל מחדש ויתחבר לרשת. להמשיך?'))return;
  try{await post('/api/wifi',{ssid:$('ssid').value,pass:$('pass').value})}catch(e){}
  toast('מופעל מחדש... התחבר שוב דרך http://uvtimer.local');
}

async function refresh(){
  try{S=await (await fetch('/api/state')).json()}catch(e){return}
  const [d,t]=S.time.split(' ');
  $('clock').textContent=S.timeValid?t:'--:--:--';
  $('date').textContent=S.timeValid?('יום '+DF[S.wday]+' · '+d.split('-').reverse().join('/')):'';
  $('timeWarn').style.display=S.timeValid?'none':'block';
  const on=S.output>0;$('state').textContent=on?'דולק':'כבוי';$('state').className='pill '+(on?'on':'off');
  $('outp').textContent='יציאה: '+S.output+'%';
  for(let i=0;i<3;i++)$('m'+i).classList.toggle('sel',S.mode==i);
  $('manualBox').style.display=S.mode==1?'block':'none';
  $('net').textContent=S.ap?('מצב נקודת גישה (UV-Timer) · '+S.ip):('מחובר ל-'+S.ssid+' · '+S.ip+' · '+S.rssi+'dBm');
  if(!loaded){
    loaded=true;slots=S.slots;renderSlots();
    $('mlevel').value=S.manualLevel;$('mlv').textContent=S.manualLevel+'%';
    $('freq').value=S.freq;$('invert').value=S.invert?1:0;$('maxLevel').value=S.maxLevel;$('tz').value=S.tz;$('ssid').value=S.ssid;
    if(!S.timeValid)syncTime(); // auto-set clock from the phone when there is no internet time
  }
}
window.onbeforeunload=e=>{if(dirty){e.preventDefault();e.returnValue=''}};
refresh();setInterval(refresh,1000);
</script>
</body>
</html>)HTML";
