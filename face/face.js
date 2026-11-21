/* HORIZON's face: the sky (sky.js, Mixtape 2's THE SKY) driven by how loud the plugin plays, and the knobs.
   In the plugin, each knob is a JUCE SliderState (JUCE's own index.js, built in beside this file), joined to the plugin parameter
   of the same name: dragging it moves the parameter (the DAW records it), and a move in the DAW (automation) turns it here.
   The plugin sends a "sky" event 30 times a second: {level, pedal, hit (a key's velocity, when one was struck), status}.
   Opened in a plain browser (no plugin), the knobs keep their own values and a click on the sky plays a pretend note. */
const IN_PLUGIN = !!(window.__JUCE__ && window.__JUCE__.backend);
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));

/* ---- the knobs ---- */
const KNOBS = {   /* each one's range as the plugin has it (for the browser on its own, and for the value under the knob) */
  level:   { start: -48, end: 6, skew: 2, def: 0, text: v => v <= -47.9 ? '-INF DB' : v.toFixed(1) + ' DB' },
  hall:    { start: 0, end: 1, skew: 1, def: .3, text: v => Math.round(v * 100) + '%' },
  release: { start: .06, end: 3, skew: .4, def: .35, text: v => v < 1 ? Math.round(v * 1000) + ' MS' : v.toFixed(2) + ' S' },
};
/* a parameter's value ↔ 0…1 as the plugin's NormalisableRange does it (with its skew) */
const toNorm = (k, v) => Math.pow(clamp((v - k.start) / (k.end - k.start), 0, 1), k.skew);
const fromNorm = (k, n) => k.start + Math.pow(clamp(n, 0, 1), 1 / k.skew) * (k.end - k.start);

let juce = null;
if (IN_PLUGIN) juce = await import('./index.js');
const lcdTop = document.querySelector('.lcd b'), lcdSub = document.querySelector('.lcd span');
let status = IN_PLUGIN ? 'TUNING...' : '', showT = 0;
function lcd() { lcdTop.textContent = 'SALAMANDER GRAND'; lcdSub.textContent = status || 'PLAY FROM YOUR KEYBOARD'; }
lcd();

for (const el of document.querySelectorAll('.kn')) {
  const id = el.dataset.k, k = KNOBS[id], cap = el.querySelector('.cap'), em = el.querySelector('em');
  /* the plugin's own state for this knob, or (in a browser) a stand-in with the same calls */
  const st = juce ? juce.getSliderState(id) : (() => { let v = k.def; const ls = []; return {
    getScaledValue: () => v, getNormalisedValue: () => toNorm(k, v), setNormalisedValue: n => { v = fromNorm(k, n); ls.forEach(f => f()); },
    sliderDragStarted() {}, sliderDragEnded() {}, valueChangedEvent: { addListener: f => ls.push(f) }, properties: {} }; })();
  const paint = () => { const n = st.getNormalisedValue(), v = st.getScaledValue();
    cap.style.setProperty('--v', n.toFixed(4)); cap.querySelector('i').style.transform = `rotate(${(-135 + 270 * n).toFixed(1)}deg)`; em.textContent = k.text(v); };
  st.valueChangedEvent.addListener(paint);
  if (st.propertiesChangedEvent) st.propertiesChangedEvent.addListener(paint);
  paint();
  /* drag up and down (Shift: finer), scroll, double-click for where it starts */
  el.addEventListener('pointerdown', e => {
    if (e.button !== 0) return;
    e.preventDefault(); el.setPointerCapture(e.pointerId); st.sliderDragStarted();
    let y = e.clientY, n = st.getNormalisedValue();
    const move = ev => { n = clamp(n + (y - ev.clientY) / (ev.shiftKey ? 900 : 220), 0, 1); y = ev.clientY; st.setNormalisedValue(n); paint(); };
    const up = () => { el.removeEventListener('pointermove', move); el.removeEventListener('pointerup', up); el.removeEventListener('pointercancel', up); st.sliderDragEnded(); };
    el.addEventListener('pointermove', move); el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up);
  });
  el.addEventListener('wheel', e => { e.preventDefault(); st.sliderDragStarted(); st.setNormalisedValue(clamp(st.getNormalisedValue() - Math.sign(e.deltaY) * .02, 0, 1)); st.sliderDragEnded(); paint(); }, { passive: false });
  el.addEventListener('dblclick', () => { st.sliderDragStarted(); st.setNormalisedValue(toNorm(k, k.def)); st.sliderDragEnded(); paint(); });
}

/* ---- the face fills the window: laid out 1400 x 788, scaled to fit (as HORIZON's full screen) ---- */
const ui = document.querySelector('.ui');
function fit() { const f = Math.min(innerWidth / 1400, innerHeight / 788); ui.style.transform = `translateX(-50%) scale(${f.toFixed(4)})`; }
addEventListener('resize', () => { fit(); size(); }); fit();

/* ---- the sky (as in HORIZON: its canvas three quarters of its size on the screen; 30 frames a second while it sounds, 8 at rest) ---- */
const skyc = document.querySelector('canvas.sky');
let sky = null, simT = 0, drift = 0, ph = 0, slow = 0, pulse = 0, nextSurge = 0, nextCloud = 0, lastDraw = 0, level = 0, pretend = 0;
const SURGE = [];
function size() { const w = Math.min(1600, Math.round(innerWidth * .75)), h = Math.max(1, Math.round(w * innerHeight / Math.max(1, innerWidth))); if (skyc.width !== w || skyc.height !== h) { skyc.width = w; skyc.height = h; } }
function surge(a, cloud) { SURGE.push({ t0: simT, dir: Math.random() < .5 ? -1 : 1, a, sp: (cloud ? .45 : .7) + Math.random() * .9, w: (cloud ? .3 : .18) + Math.random() * .3, cloud: cloud ? 1 : 0 }); if (SURGE.length > 4) SURGE.shift(); }
function hit(vel) { const v = vel / 127; pulse = Math.min(1, pulse + .2 + .5 * v); if (v > .75 && Math.random() < .35) surge(.6 + .4 * Math.random()); }
function step(dt) {
  simT += dt; drift += dt; pretend *= Math.exp(-dt * 1.2);
  const raw = level + pretend, want = Math.min(1.2, Math.sqrt(raw) * 2.2);          /* how loud, gently: quick to rise, slow to fall */
  slow += (want - slow) * Math.min(1, dt * (want > slow ? 2.5 : .8)); pulse *= Math.exp(-dt * 2.2); ph += dt * (.25 + 1.3 * slow);
  if (simT > nextSurge) { if (nextSurge) surge(.3 + .5 * slow); nextSurge = simT + 6 + Math.random() * 12; }   /* now and then by themselves */
  if (simT > nextCloud) { if (nextCloud) surge(.45 + .55 * slow, 1); nextCloud = simT + 4 + Math.random() * 9; }
  for (let i = SURGE.length - 1; i >= 0; i--) if (simT - SURGE[i].t0 > 14) SURGE.splice(i, 1);
}
function draw() {
  const asp = skyc.width / skyc.height, sw = new Float32Array(16);
  SURGE.forEach((s, i) => { const a = simT - s.t0; sw[i * 4] = s.dir > 0 ? -.4 + a * s.sp : asp + .4 - a * s.sp; sw[i * 4 + 1] = s.a * Math.min(1, a / (s.cloud ? .8 : .3)) * Math.exp(-a * (s.cloud ? .25 : .35)); sw[i * 4 + 2] = s.w; sw[i * 4 + 3] = s.cloud; });
  try { sky.draw({ t: drift, ph, lev: slow, pulse, sw }); } catch (e) { console.warn('HORIZON sky', e); sky = null; }
}
function frame(now) {
  if (!sky) return;
  const t = now / 1000, fps = slow > .02 || pulse > .02 ? 30 : 8;
  if (!lastDraw || t - lastDraw >= 1 / fps - .004) { step(Math.min(.15, lastDraw ? t - lastDraw : 1 / 30)); lastDraw = t; draw(); }
  requestAnimationFrame(frame);
}
async function startSky() {
  size();
  try { sky = window.HZ_skyGL(skyc); } catch (e) { console.warn('HORIZON sky: no WebGL2, the picture alone', e); skyc.style.display = 'none'; return; }
  requestAnimationFrame(frame);
  const im = new Image();
  im.onload = () => { if (sky) sky.photo({ im, sky: window.HZ_SKY, refl: window.HZ_REFL, y0: window.HZ_MIRROR, fire: window.HZ_FIRE }); };
  im.src = 'mountains.jpg';
}
startSky();

/* ---- what the plugin says ---- */
const light = { pedal: document.querySelector('[data-l="pedal"]'), kin: document.querySelector('[data-l="kin"]') };
let kinT = 0;
function onSky(e) {
  if (!e) return;
  level = +e.level || 0;
  light.pedal.classList.toggle('on', !!e.pedal);
  if (e.hit != null) { hit(+e.hit); light.kin.classList.add('on'); clearTimeout(kinT); kinT = setTimeout(() => light.kin.classList.remove('on'), 120); }
  if (e.status != null && String(e.status) !== status) { status = String(e.status); lcd(); }
}
if (IN_PLUGIN) window.__JUCE__.backend.addEventListener('sky', onSky);
else skyc.addEventListener('pointerdown', () => { pretend = Math.min(.6, pretend + .12); onSky({ level: 0, hit: 100 }); });   /* in a browser: a pretend note */
