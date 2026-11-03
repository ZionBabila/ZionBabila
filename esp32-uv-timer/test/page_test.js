// Browser tests for the control page (uv_timer/page.h) against a simulated device.
// Run: node page_test.js   (needs Node + Playwright with Chromium)
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const src = fs.readFileSync(path.join(__dirname, '../uv_timer/page.h'), 'utf8');
const HTML = src.split('R"HTML(')[1].split(')HTML"')[0];

let pass = 0, fail = 0;
const check = (cond, msg) => { if (cond) pass++; else { fail++; console.log('  FAIL ' + msg); } };

const F = ['d', 's', 'u', 'l', 'fi', 'fo', 'f', 'on', 'p', 'iu', 'ih', 'id', 'io'];
const parseEvents = e => e.split(';').filter(Boolean).map(x => { const v = x.split(',').map(Number); const o = {}; F.forEach((k, i) => o[k] = v[i]); return o; });

// A fake device. `opts.offline` makes every request fail.
function device(opts = {}) {
  const st = Object.assign({ now: 0, timeSet: false, rtc: false, mode: 0, manualLevel: 80, output: 0, maxLevel: 100, freq: 1000,
    invert: false, sensors: 0, tlimit: 50, tcap: 100, fault: false, temps: [], ssid: 'UV-Timer', events: [] }, opts.state || {});
  const dev = { st, posts: [], offline: false, failSave: false };
  dev.route = async r => {
    const u = r.request().url(), q = new URLSearchParams(r.request().postData() || '');
    if (u.includes('/api/') && dev.offline) return r.abort();
    if (u.endsWith('/api/state')) return r.fulfill({ json: st });
    if (u.includes('/api/')) {
      dev.posts.push({ url: u.split('/api/')[1], q: Object.fromEntries(q) });
      if (u.endsWith('/api/events')) { if (dev.failSave) return r.abort(); st.events = parseEvents(q.get('e')); }
      if (u.endsWith('/api/time')) { st.now = +q.get('t'); st.timeSet = true; }
      if (u.endsWith('/api/mode')) { st.mode = +q.get('mode'); st.manualLevel = +q.get('level'); }
      if (u.endsWith('/api/wifi') && q.get('pass') && q.get('pass').length < 8) return r.fulfill({ status: 400, body: 'bad' });
      return r.fulfill({ body: 'OK' });
    }
    return r.fulfill({ contentType: 'text/html', body: HTML });
  };
  return dev;
}

async function open(browser, dev, width = 390) {
  const p = await browser.newPage({ viewport: { width, height: 844 }, timezoneId: 'Asia/Jerusalem' });
  p.errors = [];
  p.on('pageerror', e => p.errors.push(e.message));
  p.on('dialog', d => d.accept());
  await p.route('**/*', dev.route);
  await p.goto('http://192.168.4.1/');
  await p.waitForTimeout(600);
  return p;
}
const toast = p => p.textContent('#toast');
const lastPost = (dev, url) => [...dev.posts].reverse().find(x => x.url === url);
const T0 = Math.floor(Date.UTC(2026, 10, 2, 10, 0, 0) / 1000);   // Mon 2 Nov 2026 10:00 (device local)

const tests = {
  async clock_auto_sync_when_not_set(b) {
    const dev = device(); const p = await open(b, dev);
    const t = lastPost(dev, 'time');
    check(t, 'page should set the clock automatically');
    // phone runs in Asia/Jerusalem: device must get local time, not UTC
    const jlm = Math.floor(Date.now() / 1000) + 2 * 3600;   // Israel is UTC+2 in November
    check(t && Math.abs(+t.q.t - jlm) < 5, 'clock sent as phone local time');
    check(!(await p.isVisible('#clkWarn')), 'warning hidden after sync');
    check(p.errors.length === 0, 'no JS errors: ' + p.errors);
  },
  async manual_time(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev);
    await p.click('text=Set manually');
    await p.fill('#tDate', '2027-03-05'); await p.fill('#tTime', '07:08:09');
    await p.click('text=Set clock'); await p.waitForTimeout(200);
    check(+lastPost(dev, 'time').q.t === Date.UTC(2027, 2, 5, 7, 8, 9) / 1000, 'manual time value');
  },
  async add_steady_event(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev);
    await p.click('text=+ Add event');
    await p.click('text=Mon–Fri');
    await p.fill('#evStart', '18:30'); await p.fill('#evH', '1'); await p.fill('#evM', '15');
    await p.fill('#evF', '7.6');
    await p.click('.actions .pri'); await p.waitForTimeout(300);
    const e = parseEvents(lastPost(dev, 'events').q.e)[0];
    check(e.d === 62 && e.s === 1110 && e.u === 75 && e.fi === 1 && e.fo === 1 && e.f === 8 && e.on === 1 && e.p === 0, 'steady payload ' + JSON.stringify(e));
    check((await p.textContent('#evList')).includes('18:30 – 19:45'), 'event listed');
    check((await toast(p)).includes('saved'), 'saved toast');
  },
  async validation_blocks_bad_input(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev);
    const tryIt = async (fn, msgPart) => {
      await p.click('text=+ Add event'); await fn(); const n = dev.posts.length;
      await p.click('.actions .pri'); await p.waitForTimeout(100);
      check(dev.posts.length === n, 'blocked: ' + msgPart);
      check((await toast(p)).includes(msgPart), 'message: ' + msgPart + ' got ' + (await toast(p)));
      await p.click('.actions button:has-text("Cancel")');
    };
    await tryIt(() => p.click('text=Clear'), 'at least one day');
    await tryIt(async () => { await p.fill('#evH', '0'); await p.fill('#evM', '0'); }, 'Duration');
    await tryIt(async () => { await p.fill('#evH', '24'); await p.fill('#evM', '1'); }, 'Duration');
    await tryIt(() => p.fill('#evF', '0'), 'Fade length');
    await tryIt(async () => { await p.click('#evP button[data-v="1"]'); for (const id of ['#ivU', '#ivH', '#ivD', '#ivO']) await p.fill(id, '0'); }, 'Interval times');
    await tryIt(async () => { await p.click('#evP button[data-v="1"]'); await p.fill('#ivU', '-5'); }, 'Interval times');
  },
  async interval_event_and_preview(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev);
    await p.click('text=+ Add event');
    await p.fill('#evStart', '08:00'); await p.fill('#evH', '4'); await p.fill('#evM', '0');
    await p.click('#evP button[data-v="1"]');
    check(await p.isVisible('#ivBox') && !(await p.isVisible('#steadyBox')), 'interval fields shown, steady hidden');
    check((await p.textContent('#ivHint')).includes('repeats 80×'), 'cycle hint: ' + await p.textContent('#ivHint'));
    check(!(await p.textContent('#evEnd')).includes('shortened'), 'no fade warning in interval mode');
    check((await p.$$('#ivWave polyline')).length === 1, 'wave preview drawn');
    await p.click('.actions .pri'); await p.waitForTimeout(300);
    const e = parseEvents(lastPost(dev, 'events').q.e)[0];
    check(e.p === 1 && e.iu === 60 && e.ih === 0 && e.id === 60 && e.io === 60 && e.u === 240, 'interval payload ' + JSON.stringify(e));
    check((await p.textContent('#stSub')).includes('Interval running until 12:00'), 'status: ' + await p.textContent('#stSub'));
  },
  async edit_toggle_duplicate_delete(b) {
    const ev = { d: 2, s: 1080, u: 60, l: 80, fi: 1, fo: 0, f: 5, on: 1, p: 0, iu: 60, ih: 0, id: 60, io: 60 };
    const dev = device({ state: { now: T0, timeSet: true, events: [ev] } }); const p = await open(b, dev);
    await p.click('.ev .sw span'); await p.waitForTimeout(300);
    check(parseEvents(lastPost(dev, 'events').q.e)[0].on === 0, 'toggle pauses');
    await p.click('.ev .grow'); await p.click('#evDup'); await p.fill('#evStart', '20:00');
    await p.click('.actions .pri'); await p.waitForTimeout(300);
    const list = parseEvents(lastPost(dev, 'events').q.e);
    check(list.length === 2 && list[1].s === 1200 && list[0].s === 1080, 'duplicate adds a copy, original kept');
    await p.click('.ev .grow >> nth=0'); await p.click('#evDel'); await p.waitForTimeout(300);
    await p.click('.ev .grow >> nth=0'); await p.click('#evDel'); await p.waitForTimeout(300);
    check(lastPost(dev, 'events').q.e === '', 'deleting last event sends empty list');
    check((await p.textContent('#evList')).includes('No events yet'), 'empty state');
  },
  async max_20_events(b) {
    const ev = { d: 2, s: 60, u: 10, l: 80, fi: 0, fo: 0, f: 5, on: 1, p: 0, iu: 60, ih: 0, id: 60, io: 60 };
    const dev = device({ state: { now: T0, timeSet: true, events: Array(20).fill(ev) } }); const p = await open(b, dev);
    await p.click('text=+ Add event');
    check((await toast(p)).includes('Maximum 20'), 'limit message');
    check(!(await p.isVisible('#evSheet.open')), 'editor not opened');
  },
  async offline_banner_and_failed_save(b) {
    const ev = { d: 2, s: 1080, u: 60, l: 80, fi: 1, fo: 0, f: 5, on: 1, p: 0, iu: 60, ih: 0, id: 60, io: 60 };
    const dev = device({ state: { now: T0, timeSet: true, events: [ev] } }); const p = await open(b, dev);
    dev.failSave = true;
    await p.click('.ev .sw span'); await p.waitForTimeout(300);
    check((await toast(p)).includes('Could not save'), 'failed save message');
    check(dev.st.events[0].on === 1, 'device unchanged');
    check(await p.isChecked('.ev .sw input'), 'list shows the real device state after a failed save');
    dev.failSave = false; dev.offline = true;
    await p.waitForTimeout(6500);
    check(await p.isVisible('#offline'), 'offline banner after lost connection');
    dev.offline = false; await p.waitForTimeout(3500);
    check(!(await p.isVisible('#offline')), 'banner hides when back');
  },
  async settings_and_wifi(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev);
    await p.click('summary');
    await p.selectOption('#sens', '2'); await p.fill('#tlim', '55'); await p.fill('#freq', '500');
    await p.click('text=Save settings'); await p.waitForTimeout(200);
    const s = lastPost(dev, 'settings').q;
    check(s.sensors === '2' && s.tlimit === '55' && s.freq === '500', 'settings payload ' + JSON.stringify(s));
    await p.fill('#pass', 'short'); await p.click('text=Save & restart'); await p.waitForTimeout(100);
    check(!lastPost(dev, 'wifi'), 'short password blocked in the page');
    await p.fill('#ssid', 'Lab'); await p.fill('#pass', '12345678'); await p.click('text=Save & restart'); await p.waitForTimeout(200);
    check(lastPost(dev, 'wifi').q.ssid === 'Lab', 'wifi saved');
  },
  async manual_mode(b) {
    const dev = device({ state: { now: T0, timeSet: true, output: 50 } }); const p = await open(b, dev);
    await p.click('#m1'); await p.waitForTimeout(200);
    check(lastPost(dev, 'mode').q.mode === '1', 'always on sent');
    check(await p.isVisible('#manBox'), 'brightness slider shown');
    check(await p.isVisible('#uvOn'), 'UV-on warning visible while on');
  },
  async layout_small_phone(b) {
    const dev = device({ state: { now: T0, timeSet: true } }); const p = await open(b, dev, 320);
    const over = await p.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 1);
    check(!over, 'no sideways scrolling at 320 px');
    await p.click('text=+ Add event');
    const over2 = await p.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 1);
    check(!over2, 'editor fits at 320 px');
    check(p.errors.length === 0, 'no JS errors');
  },
};

(async () => {
  const b = await chromium.launch();
  for (const [name, fn] of Object.entries(tests)) {
    const before = fail;
    try { await fn(b); } catch (e) { fail++; console.log('  FAIL ' + name + ': ' + e.message.split('\n')[0]); }
    console.log((fail === before ? 'ok   ' : 'FAIL ') + name);
  }
  await b.close();
  console.log(`\n${pass} checks passed, ${fail} failed`);
  process.exit(fail ? 1 : 0);
})();
