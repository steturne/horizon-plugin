/* HORIZON's face in the plugin: Mixtape 2's HORIZON (mixtape2/Devices/horizon.js) as at full screen: the sky (sky.js, THE SKY)
   driven by how loud the plugin plays, and on clear glass the bar (HORIZON, the PEDAL and KEYS IN lights, the screen, PIANO /
   FX ONLY / OFF, the drawers SHAPER REVERSE PAD EFFECTS CHANNEL and STAGE, MASTER); a drawer's strips and its MORE panel (or STAGE:
   all twelve strips) rise on a pane of glass. The tables below (STRIPS, DETAILS, names, tips, value words) are HORIZON's own.
   Every control is a plugin parameter (plugin/Params.h, the same IDs: HORIZON's setting name with its dots made underscores, the
   PAD's sound pad.P.x as pad_x), reached through JUCE's own index.js (built in beside this file): a knob is a SliderState, a switch
   a ToggleState, a mode a ComboBoxState, each joined to the parameter of the same name, so a move here is a move in the DAW and an
   automation in the DAW moves it here. The plugin sends a "sky" event 30 times a second: {level, pedal, hit, status}.
   Opened in a plain browser (no plugin), the controls keep their own values and a click on the sky plays a pretend note. */
const IN_PLUGIN = !!(window.__JUCE__ && window.__JUCE__.backend);
const juce = IN_PLUGIN ? await import('./index.js') : null;
const clamp = (v, a, b) => Math.min(b, Math.max(a, v)), pct = v => Math.round(v * 100) + '%', sec = s => s < 1 ? Math.round(s * 1000) + ' MS' : s.toFixed(2) + ' S';

/* ---- HORIZON's tables ---- */
const SHAPE0 = { punch: .6, bright: .5, len: .53, hammer: 0, tail: .6, soft: 0, warmth: 0, darken: 0, sustain: 0, sparkle: 0, tubeS: 1, delS: 1, revS: 1, tubeT: 1, delT: 1, revT: 1 };
const FAM = { pno: ['PIANO', '#FF7A3C'], pad: ['PAD', '#C29BFF'], fx: ['EFFECTS', '#7AB8FF'], out: ['OUT', '#D8D8E8'] };
const STRIPS = {
  piano: { fam: 'pno', title: 'PIANO', modes: [['p.on', 'ON'], ['p.fx', 'FX'], ['p.off', 'OFF']], knobs: ['damp', 'release', 'shape.sparkle'], fader: 'level' },
  strike: { fam: 'pno', title: 'STRIKE', modes: [['sk.soft', 'SOFT'], ['sk.hard', 'HARD']], knobs: ['shape.bright', 'shape.hammer', 'shape.len'], fader: 'shape.punch' },
  tail: { fam: 'pno', title: 'TAIL', modes: [['tl.nat', 'NATURAL'], ['tl.end', 'ENDLESS']], knobs: ['shape.warmth', 'shape.sustain', 'shape.soft'], fader: 'shape.tail' },
  pad: { fam: 'pad', title: 'PAD', sw: 'pad', name: 'pp', knobs: ['pad.P.swell', 'pad.P.tone', 'pad.duck'], fader: 'pad.level' },
  drone: { fam: 'pad', title: 'DRONE', name: 'dk', toggles: [['dr.root', 'ROOT'], ['dr.fifth', 'FIFTH'], ['dr.oct', 'OCTAVE'], ['dr.sub', 'SUB']], fader: 'pad.dlevel' },
  plate: { fam: 'out', title: 'PLATE', sw: 'plate', knobs: ['plate.decay', 'plate.pre', 'plate.low'], fader: 'plate.mix' },
  reverse: { fam: 'pno', title: 'REVERSE', sw: 'rev', modes: [['rev.back', 'BACK'], ['rev.pnobk', 'P+B'], ['rev.echo', 'ECHO']], knobs: ['rev.time', 'rev.swell', 'rev.rep'], fader: 'rev.mix' },
  tube: { fam: 'fx', title: 'TUBE', sw: 'tube', knobs: ['tube.drive', 'tube.warmth'], fader: 'tube.level' },
  tape: { fam: 'fx', title: 'TAPE', sw: 'tape', knobs: ['tape.wobble', 'tape.sat', 'tape.hiss'] },
  delay: { fam: 'fx', title: 'DELAY', sw: 'del', modes: [['del.1/4', '1/4'], ['del.1/8d', '1/8·'], ['del.1/8', '1/8']], knobs: ['del.time', 'del.rep', 'del.duck'], fader: 'del.mix' },
  reverb: { fam: 'fx', title: 'REVERB', sw: 'verb', modes: [['s:freeze', 'FREEZE']], knobs: ['verb.size', 'verb.shim', 'verb.duck'], fader: 'verb.mix' },
  chan: { fam: 'out', title: 'CHANNEL', modes: [['chan.mono', 'MONO']], knobs: ['chan.glue', 'chan.low', 'chan.width'], fader: 'chan.out' } };
const ORDER = ['piano', 'strike', 'tail', 'reverse', 'pad', 'drone', 'tube', 'tape', 'delay', 'reverb', 'plate', 'chan'];
const CHARS = { warm: 'WARM', glass: 'GLASS', choir: 'CHOIR', strings: 'STRINGS', air: 'AIR', organ: 'ORGAN', bowl: 'BOWL', shruti: 'SHRUTI' };
const MOTIONS = ['still', 'breathe', 'shimmer', 'drift', 'pulse', 'wander'], VO = ['one', 'oct', 'fifth', 'wide', 'sus'];
const VO_NAME = { one: 'SINGLE', oct: 'OCTAVE', fifth: 'FIFTH', wide: 'WIDE', sus: 'SUS' }, NM = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const DETAILS = {
  shaper: { strips: ['piano', 'strike', 'tail'], title: 'SHAPER · MORE', rows: [['STRIKE INTO', ['shape.tubeS', 'shape.delS', 'shape.revS']], ['TAIL INTO', ['shape.tubeT', 'shape.delT', 'shape.revT']], ['TAIL', ['shape.darken']]] },
  pad: { strips: ['pad', 'drone'], title: 'PAD · MORE', top: [['CHARACTER', Object.keys(CHARS).map(c => ['pc.' + c, CHARS[c]])], ['MOTION', MOTIONS.map(x => ['pm.' + x, x.toUpperCase()])]],
    rows: [['SOUND', ['pad.P.width', 'pad.P.amt', 'pad.P.speed', 'pad.P.space']], ['', ['pad.P.tex', 'pad.P.sub', 'pad.P.air', 'pad.P.spark']], ['MIX', ['pad.rev', 'pad.low']]],
    bottom: [['VOICING', VO.map(x => ['pv.' + x, VO_NAME[x]])], ['PLAYED BY', [['pr.all', 'ALL KEYS'], ['pr.left', 'LEFT HAND'], ['po.-1', 'OCT −'], ['po.0', 'OCT 0'], ['po.1', 'OCT +']]]] },
  reverse: { strips: ['reverse'], title: 'REVERSE · MORE', top: [['TIME', [['rev.sync', 'IN BEATS OF THE TEMPO']]], ['NOTES', [['rev.rings', 'RING ON AFTER THE KEY']]]] },
  effects: { strips: ['tube', 'tape', 'delay', 'reverb'], title: 'EFFECTS · MORE', top: [['DELAY', [['del.sync', 'IN BEATS OF THE TEMPO']]]], rows: [['DELAY', ['del.tone']], ['REVERB', ['verb.bloom', 'verb.pre', 'verb.low']]] },
  chan: { strips: ['plate', 'chan'], title: 'CHANNEL · MORE', rows: [['TONE', ['chan.body', 'chan.pres', 'chan.air']], ['GLUE', ['chan.blend']], ['PLATE', ['plate.high', 'plate.duck']]] } };
const KNAME = { level: 'PIANO LEVEL', damp: 'DAMP', release: 'RELEASE', 'tube.drive': 'TUBE DRIVE', 'tube.warmth': 'TUBE WARMTH', 'tube.level': 'TUBE LEVEL', 'rev.time': 'REVERSE TIME', 'rev.swell': 'REVERSE SWELL',
  'rev.rep': 'REVERSE REPEATS', 'rev.mix': 'REVERSE MIX', 'del.time': 'DELAY TIME', 'del.rep': 'DELAY REPEATS', 'del.tone': 'DELAY TONE', 'del.mix': 'DELAY MIX', 'del.duck': 'DELAY DUCK', 'verb.mix': 'REVERB MIX',
  'verb.size': 'REVERB SIZE', 'verb.shim': 'SHIMMER', 'verb.bloom': 'BLOOM', 'verb.pre': 'PRE-DELAY', 'verb.low': 'REVERB LOW CUT', 'verb.duck': 'REVERB DUCK',
  'shape.punch': 'PUNCH', 'shape.bright': 'BRIGHT', 'shape.len': 'STRIKE LENGTH', 'shape.hammer': 'HAMMER', 'shape.tail': 'TAIL LEVEL', 'shape.soft': 'SOFT TAIL', 'shape.warmth': 'WARMTH',
  'shape.darken': 'DARKEN', 'shape.sustain': 'SUSTAIN', 'shape.sparkle': 'SPARKLE', 'shape.tubeS': 'STRIKE → TUBE', 'shape.delS': 'STRIKE → DELAY', 'shape.revS': 'STRIKE → REVERB',
  'shape.tubeT': 'TAIL → TUBE', 'shape.delT': 'TAIL → DELAY', 'shape.revT': 'TAIL → REVERB', 'pad.level': 'PAD LEVEL', 'pad.P.swell': 'PAD SWELL', 'pad.P.tone': 'PAD TONE', 'pad.P.width': 'PAD WIDTH',
  'pad.P.amt': 'PAD MOTION', 'pad.P.speed': 'PAD SPEED', 'pad.P.space': 'PAD SPACE', 'pad.P.tex': 'PAD TEXTURE', 'pad.P.sub': 'PAD SUB', 'pad.P.air': 'PAD AIR', 'pad.P.spark': 'PAD SPARKLE',
  'pad.rev': 'PAD INTO HALL', 'pad.low': 'PAD LOW CUT', 'pad.duck': 'PAD DUCK', 'tape.wobble': 'TAPE WOBBLE', 'tape.sat': 'TAPE SATURATION', 'tape.hiss': 'TAPE HISS',
  'chan.glue': 'GLUE', 'chan.blend': 'GLUE BLEND', 'chan.low': 'LOW CUT', 'chan.body': 'BODY', 'chan.pres': 'PRESENCE', 'chan.air': 'AIR', 'chan.width': 'WIDTH', 'chan.out': 'MASTER', 'pad.dlevel': 'DRONE LEVEL', 'plate.mix': 'PLATE MIX', 'plate.decay': 'PLATE DECAY', 'plate.pre': 'PLATE PRE-DELAY',
  'plate.low': 'PLATE LOW CUT', 'plate.high': 'PLATE HIGH CUT', 'plate.duck': 'PLATE DUCK' };
const LABEL = { level: 'LEVEL', 'shape.punch': 'PUNCH', 'shape.len': 'LENGTH', 'shape.tail': 'LEVEL', 'shape.tubeS': 'TUBE', 'shape.delS': 'DELAY', 'shape.revS': 'REVERB', 'shape.tubeT': 'TUBE',
  'shape.delT': 'DELAY', 'shape.revT': 'REVERB', 'pad.level': 'LEVEL', 'pad.P.swell': 'SWELL', 'pad.P.tone': 'TONE', 'pad.P.width': 'WIDTH', 'pad.P.amt': 'MOTION', 'pad.P.speed': 'SPEED',
  'pad.P.space': 'SPACE', 'pad.P.tex': 'TEXTURE', 'pad.P.sub': 'SUB', 'pad.P.air': 'AIR', 'pad.P.spark': 'SPARKLE', 'pad.rev': 'INTO HALL', 'pad.low': 'LOW CUT', 'pad.duck': 'DUCK',
  'rev.time': 'TIME', 'rev.swell': 'SWELL', 'rev.rep': 'REPEATS', 'rev.mix': 'MIX', 'tube.drive': 'DRIVE', 'tube.warmth': 'WARMTH', 'tube.level': 'LEVEL', 'tape.wobble': 'WOBBLE', 'tape.sat': 'SATURATE',
  'tape.hiss': 'HISS', 'del.time': 'TIME', 'del.rep': 'REPEATS', 'del.tone': 'TONE', 'del.mix': 'MIX', 'del.duck': 'DUCK', 'verb.mix': 'MIX', 'verb.size': 'SIZE', 'verb.shim': 'SHIMMER', 'verb.bloom': 'BLOOM',
  'verb.pre': 'PRE-DELAY', 'verb.low': 'LOW CUT', 'verb.duck': 'DUCK', 'chan.glue': 'GLUE', 'chan.blend': 'BLEND', 'chan.low': 'LOW CUT', 'chan.body': 'BODY', 'chan.pres': 'PRESENCE', 'chan.air': 'AIR',
  'chan.width': 'WIDTH', 'chan.out': 'MASTER', 'pad.dlevel': 'LEVEL', 'plate.mix': 'MIX', 'plate.decay': 'DECAY', 'plate.pre': 'PRE-DELAY', 'plate.low': 'LOW CUT', 'plate.high': 'HIGH CUT', 'plate.duck': 'DUCK' };
const TIP = { 'shape.punch': 'how loud the hammer\'s strike is', 'shape.bright': 'the strike\'s own tone', 'shape.len': 'how long counts as the strike before the tail takes over',
  'shape.hammer': 'a harder hit: the strike from a harder-played recording, louder and brighter', 'shape.tail': 'how loud the ringing tail is',
  'shape.soft': 'the tail from a softer-played recording: rounder, warmer', 'shape.warmth': 'a filter on the tail only', 'shape.darken': 'the tail grows darker as it rings',
  'shape.sustain': 'how slowly the tail dies away; at the top, ENDLESS: it rings while the key is held', 'shape.sparkle': 'tiny glints an octave or two up over the ring (the stars)',
  damp: 'felt … clear', release: 'how fast a let-go note stops', 'pad.level': 'how loud the pad is', 'pad.P.swell': 'how slowly the pad swells in and fades away',
  'pad.P.tone': 'darker … brighter', 'pad.P.width': 'narrow … wide', 'pad.P.amt': 'how deep the MOTION moves', 'pad.P.speed': 'how slow one cycle of the MOTION is',
  'pad.P.space': 'the pad\'s own hall', 'pad.P.tex': 'breath and grit', 'pad.P.sub': 'a soft octave below', 'pad.P.air': 'breathy air on top', 'pad.P.spark': 'a glint two octaves up',
  'pad.rev': 'how much of the pad also goes into the REVERB', 'pad.low': 'takes the pad\'s lowest notes away, so it leaves room for the piano and the bass',
  'pad.duck': 'the pad dips a little each time you play and swells back as the piano rings', 'del.duck': 'the echoes step back while you play and come up in the gaps',
  'verb.duck': 'the hall steps back while you play and blooms in the gaps', 'verb.pre': 'a short wait before the hall starts, so each note lands clean',
  'verb.low': 'takes the low notes out of the hall only, so a big room stays airy', 'tape.wobble': 'the slow, gentle pitch wobble of an old tape',
  'tape.sat': 'tape\'s soft saturation', 'tape.hiss': 'a little tape hiss', 'chan.glue': 'a gentle compressor on everything together: the glue',
  'chan.blend': 'how much of the glued sound is mixed in beside the natural one (parallel)', 'chan.low': 'takes the lowest notes of everything away (they fight the bass guitar)',
  'chan.body': 'the warm low middle', 'chan.pres': 'the middle that cuts through a band', 'chan.air': 'the sparkle on top', 'chan.width': 'how wide left and right are (MONO: the same in both)',
  'chan.out': 'the level of everything HORIZON sends out', 'pad.dlevel': 'how loud the drone is, apart from the pad played by the keys',
  'plate.mix': 'how much plate is heard: a clean, quick plate reverb, as Abbey Road\'s, that stays clear on big speakers',
  'plate.decay': 'how long the plate rings', 'plate.pre': 'a short wait before the plate starts, so each note lands clean',
  'plate.low': 'takes the lows out of the plate only (Abbey Road\'s trick: about 600 Hz), so it never muddies',
  'plate.high': 'takes the very top out of the plate (about 10 kHz), so it stays smooth', 'plate.duck': 'the plate steps back while you play and comes up in the gaps' };
/* HALO's sounds for the PAD's ‹ › (the same order as the plugin's "Pad Sound" list) */
const HALO_DEF = { ch: ['warm'], lv: {}, voicing: 'one', motion: 'breathe', tone: .45, width: .65, swell: .55, amt: .35, space: .55, tex: .25, speed: .75, sub: .3, air: 0, spark: 0 };
const HALO_PRESETS = {
  'Warm blanket': { sub: .5 },
  'Glass cathedral': { ch: ['glass'], voicing: 'oct', motion: 'shimmer', tone: .7, width: .7, swell: .6, amt: .5, space: .85, tex: .05, sub: 0, spark: .3 },
  'Night choir': { ch: ['choir'], voicing: 'fifth', motion: 'drift', tone: .5, width: .6, swell: .7, amt: .4, space: .75, tex: .1, sub: .15 },
  'Cinematic strings': { ch: ['strings'], voicing: 'wide', motion: 'breathe', tone: .55, width: .85, swell: .65, amt: .3, space: .7, tex: .15, sub: .2 },
  'Ocean air': { ch: ['air'], voicing: 'oct', motion: 'drift', tone: .6, width: .9, swell: .8, amt: .6, space: .8, tex: .2, sub: .1 },
  'Pulse ribbon': { ch: ['warm'], voicing: 'fifth', motion: 'pulse', tone: .6, width: .6, swell: .3, amt: .7, space: .55, tex: .2 },
  'Dream sus': { ch: ['glass'], voicing: 'sus', motion: 'drift', tone: .62, width: .75, swell: .7, amt: .45, space: .8, tex: .1, sub: 0 },
  'Lo-fi tape pad': { ch: ['warm'], voicing: 'oct', motion: 'drift', tone: .35, width: .5, swell: .5, amt: .55, space: .45, tex: .7 },
  'Glass choir': { ch: ['glass', 'choir'], lv: { glass: .8 }, voicing: 'oct', motion: 'breathe', tone: .6, width: .8, swell: .7, amt: .35, space: .8, tex: .08, sub: .1 },
  'Harp drone': { ch: ['shruti'], motion: 'wander', tone: .5, width: .7, swell: .8, amt: .45, space: .7, tex: .15, speed: .5, sub: .3 },
  'Singing bowl': { ch: ['bowl', 'air'], lv: { air: .35 }, motion: 'breathe', tone: .6, width: .75, swell: .75, amt: .3, space: .85, tex: .05, speed: .45, sub: 0 },
  'Deep space': { ch: ['air', 'glass'], lv: { glass: .5 }, motion: 'drift', tone: .4, width: 1, swell: .9, amt: .6, space: .95, tex: .2, speed: .35, sub: .45 },
  'Cathedral organ': { ch: ['organ'], voicing: 'oct', motion: 'still', tone: .55, width: .6, swell: .35, amt: .2, space: .9, tex: .05, sub: .3 },
  'Monastery': { ch: ['choir', 'strings'], lv: { strings: .5 }, motion: 'drift', tone: .45, width: .7, swell: .85, amt: .35, space: .85, tex: .1, speed: .4, sub: .35 } };
const PAD_NAMES = Object.keys(HALO_PRESETS);
/* where each knob starts (double-click) */
const DEFK = { level: .75, damp: .6, release: .25, 'tube.drive': .3, 'tube.warmth': .5, 'tube.level': .5, 'rev.time': .35, 'rev.swell': .45, 'rev.rep': .45, 'rev.mix': .5,
  'del.time': .45, 'del.rep': .3, 'del.tone': .5, 'del.mix': .25, 'del.duck': 0, 'verb.mix': .4, 'verb.size': .6, 'verb.shim': 0, 'verb.bloom': 0, 'verb.pre': .12, 'verb.low': .35, 'verb.duck': 0,
  'pad.level': .5, 'pad.rev': .25, 'pad.low': 0, 'pad.duck': 0, 'pad.dlevel': .6, 'tape.wobble': .3, 'tape.sat': .3, 'tape.hiss': .1,
  'chan.low': 0, 'chan.body': .5, 'chan.pres': .5, 'chan.air': .5, 'chan.glue': .25, 'chan.blend': .5, 'chan.width': .667, 'chan.out': .833,
  'plate.mix': .35, 'plate.decay': .5, 'plate.pre': .15, 'plate.low': .66, 'plate.high': .67, 'plate.duck': 0 };
for (const [k, v] of Object.entries(SHAPE0)) DEFK['shape.' + k] = v;
for (const k of ['swell', 'tone', 'width', 'amt', 'speed', 'space', 'tex', 'sub', 'air', 'spark']) DEFK['pad.P.' + k] = HALO_DEF[k];
/* what does not sound yet in the plugin (coming in its next steps): its controls work and are kept, the screen says so */
const SOON = k => false;   /* (everything sounds now; kept for anything new that is shown before it sounds) */

/* ---- the plugin's parameters (Params.h), each a JUCE state; in a plain browser, a stand-in keeping its own value ---- */
const TOGGLES = { rev_on: 0, rev_sync: 0, rev_rings: 1, tube_on: 0, tape_on: 0, del_on: 0, del_sync: 1, verb_on: 1, verb_freeze: 0, plate_on: 0, chan_mono: 0, pad_on: 0,
  dr_root: 0, dr_fifth: 0, dr_oct: 0, dr_sub: 0 };
for (const c of Object.keys(CHARS)) TOGGLES['pad_ch_' + c] = c === 'warm' ? 1 : 0;
const CHOICES = { piano: 0, rev_kind: 1, del_div: 1, pad_preset: 0, pad_motion: 1, pad_voicing: 0, pad_range: 0, pad_oct: 1, dr_key: 0 };
const idOf = k => k.startsWith('pad.P.') ? 'pad_' + k.slice(6) : k.replace(/\./g, '_');
const kindOf = id => id in TOGGLES ? 'toggle' : id in CHOICES ? 'choice' : 'knob';
const ctls = new Map();
let repaint = () => {};
function ctl(id) {
  let c = ctls.get(id); if (c) return c;
  const kind = kindOf(id);
  if (juce) {
    const st = kind === 'toggle' ? juce.getToggleState(id) : kind === 'choice' ? juce.getComboBoxState(id) : juce.getSliderState(id);
    c = kind === 'toggle' ? { get: () => st.getValue() ? 1 : 0, set: v => st.setValue(!!v), begin() {}, end() {} }
      : kind === 'choice' ? { get: () => st.getChoiceIndex(), set: v => st.setChoiceIndex(v), begin() {}, end() {} }
      : { get: () => st.getScaledValue(), set: v => st.setNormalisedValue(clamp(v, 0, 1)), begin: () => st.sliderDragStarted(), end: () => st.sliderDragEnded() };
    st.valueChangedEvent.addListener(() => repaint());
    if (st.propertiesChangedEvent) st.propertiesChangedEvent.addListener(() => repaint());
  } else {
    let v = kind === 'toggle' ? TOGGLES[id] : kind === 'choice' ? CHOICES[id] : (DEFK[Object.keys(DEFK).find(k => idOf(k) === id)] ?? (id.startsWith('pad_lv_') ? 1 : 0));
    c = { get: () => v, set: x => { v = x; repaint(); }, begin() {}, end() {} };
  }
  ctls.set(id, c); return c;
}
const get = k => ctl(idOf(k)).get(), tog = id => !!ctl(id).get(), cho = id => ctl(id).get();
const put = (id, v) => { const c = ctl(id); c.begin(); c.set(v); c.end(); };

/* ---- the words for a knob's value (horizon.js fmt) ---- */
const relOf = v => .06 * Math.pow(50, v), freeT = v => .03 + 1.97 * v * v, swellOf = v => .15 * Math.pow(4 / .15, v), delT = v => .06 * Math.pow(25, v), decOf = v => 1.5 * Math.pow(16, v);
const lvDb = v => -18 + 30 * v, lenOf = v => .008 * Math.pow(20, v), warmCut = v => 20000 * Math.pow(700 / 20000, v);
const plDec = v => .4 * Math.pow(10, v), plLow = v => 100 * Math.pow(15, v), plHigh = v => 3000 * Math.pow(6, v), dlvOf = v => v < .01 ? 0 : Math.pow(v / .6, 1.7);
const BEATS = ['1/16', '1/8', '1/8 ·', '1/4', '1/4 ·', '1/2', '1 BAR'], DIVS = ['1/4', '1/8 ·', '1/8'];
const sgn = d => (d >= 0 ? '+' : '') + d.toFixed(1) + ' dB';
const fmt = k => { const v = get(k);
  if (k === 'release') return sec(relOf(v)); if (k === 'damp') return v < .35 ? 'FELT' : v > .75 ? 'CLEAR' : pct(v);
  if (k === 'rev.time') return tog('rev_sync') ? BEATS[Math.round(v * (BEATS.length - 1))] : sec(freeT(v)); if (k === 'rev.swell') return sec(swellOf(v));
  if (k === 'del.time') return tog('del_sync') ? DIVS[cho('del_div')] : sec(delT(v)); if (k === 'verb.size') return decOf(v).toFixed(1) + ' s';
  if (k === 'shape.punch' || k === 'shape.tail') return sgn(lvDb(v)); if (k === 'shape.bright') return sgn((v - .5) * 24); if (k === 'shape.len') return Math.round(lenOf(v) * 1000) + ' ms';
  if (k === 'shape.warmth') return v < .02 ? 'OPEN' : warmCut(v) >= 1000 ? (warmCut(v) / 1000).toFixed(1) + ' kHz' : Math.round(warmCut(v)) + ' Hz';
  if (k === 'shape.sustain') return v < .01 ? 'NATURAL' : v >= .97 ? 'ENDLESS' : pct(v); if (/shape\.(hammer|soft|darken|sparkle)/.test(k) && v < .02) return 'OFF';
  if (k === 'chan.out') return sgn(-30 + 36 * v); if (k === 'chan.low') return v < .02 ? 'OFF' : Math.round(30 * Math.pow(12, v)) + ' Hz'; if (/^chan\.(body|pres|air)$/.test(k)) return sgn((v - .5) * 18);
  if (k === 'chan.width') return tog('chan_mono') ? 'MONO' : Math.round(v * 150) + '%'; if (k === 'verb.pre') return Math.round(v * 200) + ' ms'; if (k === 'verb.low') return Math.round(40 * Math.pow(15, v)) + ' Hz';
  if (k === 'pad.low') return v < .02 ? 'OFF' : Math.round(30 * Math.pow(13, v)) + ' Hz'; if (k === 'pad.dlevel') return v < .01 ? 'OFF' : sgn(20 * Math.log10(dlvOf(v)));
  if (k === 'plate.decay') return plDec(v).toFixed(1) + ' s'; if (k === 'plate.pre') return Math.round(v * 120) + ' ms'; if (k === 'plate.low') return Math.round(plLow(v)) + ' Hz';
  if (k === 'plate.high') return (plHigh(v) / 1000).toFixed(1) + ' kHz'; if (/duck$/.test(k) && v < .02) return 'OFF'; return pct(v); };

/* ---- the face (as horizon.js builds it) ---- */
const tipOf = k => (KNAME[k] || k) + (TIP[k] ? ': ' + TIP[k] : '');
const knob = (k, cls) => `<div class="kn${cls ? ' ' + cls : ''}" data-k="${k}" title="${tipOf(k)}"><div class="cap" data-k="${k}"><i></i></div><b>${LABEL[k] || KNAME[k]}</b><em></em></div>`;
const fader = k => `<div class="fd" data-k="${k}" title="${tipOf(k)}"><div class="track" data-k="${k}"><i></i></div><b>${LABEL[k] || KNAME[k]}</b><em></em></div>`;
const chip = ([b, l]) => b.startsWith('s:') ? `<span class="chip" data-s="${b.slice(2)}">${l}</span>` : `<span class="chip${/\.name$/.test(b) ? ' nm' : ''}" data-b="${b}">${l}</span>`;
function stripHTML(id) { const s = STRIPS[id], ks = s.knobs || [];
  const modes = s.modes ? `<div class="modes" style="grid-template-columns:repeat(${s.modes.length},1fr)">${s.modes.map(chip).join('')}</div>` :
    s.name ? `<div class="nmrow">${chip([s.name + '.prev', '‹'])}${chip([s.name + '.name', ''])}${chip([s.name + '.next', '›'])}</div>` : '<span></span>';
  const slots = s.toggles ? [`<div class="tg">${s.toggles.map(chip).join('')}</div>`, '<span></span>', '<span></span>'] : [0, 1, 2].map(i => ks[i] ? knob(ks[i]) : '<span></span>');
  return `<div class="strip" data-st="${id}" style="--c:${FAM[s.fam][1]}"><h3>${s.title}</h3>${s.sw ? `<span class="sw" data-s="${s.sw}"><i></i>ON</span>` : '<span></span>'}
    ${modes}${slots.join('')}${s.fader ? fader(s.fader) : '<span></span>'}</div>`; }
function moreHTML(id) { const d = DETAILS[id], ln = ([l, bs]) => `<div class="ln"><i>${l}</i>${bs.map(chip).join('')}</div>`, top = d.top || [], rows = d.rows || [];
  return `<div class="more" data-more="${id}"><h3 style="justify-self:start">${d.title}</h3><span></span>
    <div class="two">${top.map(ln).join('')}</div>${[0, 1, 2].map(i => rows[i] ? `<div class="kr"><i>${rows[i][0]}</i>${rows[i][1].map(k => knob(k)).join('')}</div>` : '<span></span>').join('')}
    ${d.bottom ? `<div class="two">${d.bottom.map(ln).join('')}</div>` : '<span></span>'}</div>`; }
const m = document.querySelector('.hz');
m.querySelector('.ui').innerHTML = `<div class="credit glass">Piano: Salamander Grand by Alexander Holm (CC BY 3.0)</div>
  <div class="corner glass"><span class="cb" data-b="smaller" title="a smaller window">−</span><span class="cb" data-b="bigger" title="a bigger window">+</span><span class="cb panic" data-b="panic" title="PANIC: every note stops">PANIC</span></div>
  <div class="pane glass none"></div>
  <div class="bar glass"><span class="word">HORIZON</span><span class="dots"><span class="dot"><i data-l="pedal"></i>PEDAL</span><span class="dot"><i data-l="kin"></i>KEYS IN</span></span><span class="sep"></span>
    <span class="pre"><i data-b="prev" title="the preset before">‹</i><span class="lcd"><b></b><span></span></span><i data-b="next" title="the next preset">›</i></span><span class="sep"></span>
    <span class="pill"><span data-b="p.on">PIANO</span><span data-b="p.fx">FX ONLY</span><span data-b="p.off">OFF</span></span><span class="sep"></span>
    ${[['shaper', 'SHAPER', '#FF7A3C'], ['reverse', 'REVERSE', '#FF7A3C'], ['pad', 'PAD', '#C29BFF'], ['effects', 'EFFECTS', '#7AB8FF'], ['chan', 'CHANNEL', '#D8D8E8'], ['stage', 'STAGE', '#FFE7A8']].map(([d, l, c]) => `<span class="mod" data-m="${d}" style="--c:${c}">${l}</span>`).join('')}
    <span class="sep"></span>${knob('chan.out', 'tk')}</div>`;
const $ = s => m.querySelector(s), ui = $('.ui'), drw = $('.pane');
const el1 = h => { const t = document.createElement('div'); t.innerHTML = h; return t.firstElementChild; };
const stEl = {}, moreEl = {};
for (const id of ORDER) stEl[id] = el1(stripHTML(id));
for (const id of Object.keys(DETAILS)) moreEl[id] = el1(moreHTML(id));
const parts = () => [m, ...Object.values(stEl), ...Object.values(moreEl)];
let view = 'simple', open = '';
function layout() { const st = view === 'stage', isOpen = st || !!open;
  if (isOpen) { drw.innerHTML = '';
    if (st) { const fams = document.createElement('div'); fams.className = 'fams'; let col = 1;
      for (const [f, [l, c]] of Object.entries(FAM)) { const n = ORDER.filter(id => STRIPS[id].fam === f).length; fams.insertAdjacentHTML('beforeend', `<div class="fam" style="grid-column:${col} / span ${n};--c:${c}">${l}</div>`); col += n; }
      drw.appendChild(fams); const row = document.createElement('div'); row.className = 'rowc'; for (const id of ORDER) row.appendChild(stEl[id]); drw.appendChild(row); }
    else { const d = DETAILS[open], row = document.createElement('div'); row.className = 'rowc'; for (const id of d.strips) row.appendChild(stEl[id]); row.appendChild(moreEl[open]); drw.appendChild(row); } }
  drw.classList.toggle('none', !isOpen);
  m.querySelectorAll('.mod[data-m]').forEach(b => b.classList.toggle('on', b.dataset.m === 'stage' ? st : !st && open === b.dataset.m));
  paint(); }
function openDrawer(id) { if (id === 'stage') view = view === 'stage' ? 'simple' : 'stage'; else { if (view === 'stage') view = 'simple'; open = open === id ? '' : id; } layout(); }

/* ---- the screen: what is playing, or a knob's value while it turns ---- */
let status = IN_PLUGIN ? 'TUNING...' : '', flashT = 0, flashing = false, preset = { name: 'HORIZON', i: 0, n: 0 };
const lines = (a, b) => { $('.lcd b').textContent = a || ''; $('.lcd span').textContent = b || ''; };
function show() { if (flashing) return;
  if (status) return lines(status.startsWith('NO') ? 'NO PIANO' : 'TUNING THE PIANO', status);
  lines(preset.name.toUpperCase(), ['PIANO ON', 'FX ONLY', 'PIANO OFF'][cho('piano')] + (preset.n ? ' · ' + (preset.i + 1) + ' / ' + preset.n : '')); }
const flash = (a, b) => { flashing = true; lines(a, b); clearTimeout(flashT); flashT = setTimeout(() => { flashing = false; show(); }, 1500); };
const soonNote = k => SOON(k) ? ' · SOUNDS IN A LATER STEP' : '';

/* ---- everything onto the face (whenever any value changes; once a frame at most) ---- */
let paintQ = 0;
repaint = () => { if (!paintQ) paintQ = requestAnimationFrame(() => { paintQ = 0; paint(); }); };
function paint() { const all = parts(), q = s => all.flatMap(r => [...r.querySelectorAll(s)]);
  for (const d of q('.kn')) { const k = d.dataset.k, v = clamp(+get(k) || 0, 0, 1); d.querySelector('.cap').style.setProperty('--v', v); d.querySelector('i').style.transform = `rotate(${-135 + 270 * v}deg)`; d.querySelector('em').textContent = fmt(k); }
  for (const d of q('.fd')) { const k = d.dataset.k, v = clamp(+get(k) || 0, 0, 1); d.querySelector('.track').style.setProperty('--v', v); d.querySelector('em').textContent = fmt(k); }
  /* a knob dims while what it feeds is switched off */
  const offOf = { tube: !tog('tube_on'), del: !tog('del_on'), rev: !tog('verb_on') };
  for (const d of q('.kn')) { const mm = /^shape\.(tube|del|rev)[ST]$/.exec(d.dataset.k); if (mm) d.classList.toggle('off', offOf[mm[1]]); }
  const on = { pad: tog('pad_on'), tube: tog('tube_on'), tape: tog('tape_on'), delay: tog('del_on'), reverb: tog('verb_on'), reverse: tog('rev_on'), plate: tog('plate_on') };
  for (const id of Object.keys(on)) stEl[id].querySelectorAll('.kn,.fd').forEach(d => d.classList.toggle('off', !on[id]));
  moreEl.chan.querySelectorAll('.kn[data-k^="plate."]').forEach(d => d.classList.toggle('off', !on.plate));
  const lit = (sel, yes) => q(sel).forEach(x => x.classList.toggle('lit', !!yes));
  lit('[data-b="sk.soft"]', get('shape.hammer') < .05); lit('[data-b="sk.hard"]', get('shape.hammer') >= .05);
  lit('[data-b="tl.nat"]', get('shape.sustain') < .97); lit('[data-b="tl.end"]', get('shape.sustain') >= .97);
  lit('[data-b="chan.mono"]', tog('chan_mono'));
  ['on', 'fx', 'off'].forEach((b, i) => q(`[data-b="p.${b}"]`).forEach(x => x.classList.toggle(x.classList.contains('chip') ? 'lit' : 'on', cho('piano') === i)));
  ['back', 'pnobk', 'echo'].forEach((k, i) => lit(`[data-b="rev.${k}"]`, cho('rev_kind') === i));
  lit('[data-b="rev.sync"]', tog('rev_sync')); lit('[data-b="rev.rings"]', tog('rev_rings'));
  ['1/4', '1/8d', '1/8'].forEach((k, i) => lit(`[data-b="del.${k}"]`, tog('del_sync') && cho('del_div') === i));
  lit('[data-b="del.sync"]', tog('del_sync'));
  lit('[data-s="freeze"]', tog('verb_freeze'));
  for (const [s, id] of [['pad', 'pad_on'], ['tube', 'tube_on'], ['tape', 'tape_on'], ['del', 'del_on'], ['verb', 'verb_on'], ['rev', 'rev_on'], ['plate', 'plate_on']]) q(`.sw[data-s="${s}"]`).forEach(x => x.classList.toggle('on', tog(id)));
  for (const c of Object.keys(CHARS)) lit(`[data-b="pc.${c}"]`, tog('pad_ch_' + c));
  MOTIONS.forEach((x, i) => lit(`[data-b="pm.${x}"]`, cho('pad_motion') === i)); VO.forEach((x, i) => lit(`[data-b="pv.${x}"]`, cho('pad_voicing') === i));
  lit('[data-b="pr.all"]', cho('pad_range') === 0); lit('[data-b="pr.left"]', cho('pad_range') === 1);
  [-1, 0, 1].forEach((o, i) => lit(`[data-b="po.${o}"]`, cho('pad_oct') === i));
  for (const d of ['root', 'fifth', 'oct', 'sub']) lit(`[data-b="dr.${d}"]`, tog('dr_' + d));
  q('[data-b="pp.name"]').forEach(x => x.textContent = (PAD_NAMES[cho('pad_preset')] || '').toUpperCase());
  q('[data-b="dk.name"]').forEach(x => x.textContent = 'IN ' + NM[cho('dr_key')]);
  show(); }

/* ---- what the buttons do (horizon.js button / stomp / padButton) ---- */
let hamKeep = .8, susKeep = 0;
function button(b) {
  const [g, x] = [b.slice(0, 2), b.slice(3)];
  if (x === 'name') return;   /* a name on a strip is only a label */
  if (b === 'prev' || b === 'next') { if (IN_PLUGIN) juce.getNativeFunction('preset')(b === 'next' ? 1 : -1).then(n => flash(String(n).toUpperCase(), 'PRESET')); return; }
  if (b === 'bigger' || b === 'smaller') return sizeTo(innerWidth * (b === 'bigger' ? 1.2 : 1 / 1.2));
  if (b === 'panic') { if (IN_PLUGIN) juce.getNativeFunction('panic')(); for (const d of ['root', 'fifth', 'oct', 'sub']) put('dr_' + d, 0); return flash('PANIC', 'EVERY NOTE STOPPED (AND THE DRONE)'); }
  if (b === 'sk.soft') { if (get('shape.hammer') >= .05) hamKeep = get('shape.hammer'); put('shape_hammer', 0); return flash('STRIKE', 'SOFT'); }
  if (b === 'sk.hard') { put('shape_hammer', get('shape.hammer') >= .05 ? get('shape.hammer') : hamKeep); return flash('STRIKE', 'HARD: ' + fmt('shape.hammer')); }
  if (b === 'tl.end') { if (get('shape.sustain') < .97) susKeep = get('shape.sustain'); put('shape_sustain', 1); return flash('TAIL', 'ENDLESS: IT RINGS WHILE THE KEY IS HELD'); }
  if (b === 'tl.nat') { put('shape_sustain', get('shape.sustain') >= .97 ? susKeep : get('shape.sustain')); return flash('TAIL', fmt('shape.sustain')); }
  if (b === 'chan.mono') { put('chan_mono', !tog('chan_mono')); return flash('CHANNEL', tog('chan_mono') ? 'MONO: AS A MONO CHURCH SYSTEM PLAYS IT' : 'STEREO'); }
  if (b === 'p.on' || b === 'p.fx' || b === 'p.off') { const i = ['p.on', 'p.fx', 'p.off'].indexOf(b); put('piano', i);
    return flash(['PIANO ON', 'FX ONLY', 'PIANO OFF'][i], ['THE PIANO AND ITS EFFECTS', 'ONLY THE EFFECTS OF IT', 'SILENT'][i]); }
  if (b.startsWith('rev.')) { const k = b.slice(4);
    if (k === 'sync' || k === 'rings') put('rev_' + k, !tog('rev_' + k)); else { put('rev_kind', ['back', 'pnobk', 'echo'].indexOf(k)); put('rev_on', 1); }
    return flash('REVERSE', (k === 'sync' ? (tog('rev_sync') ? 'TIME IN BEATS' : 'TIME FREE') : k === 'rings' ? (tog('rev_rings') ? 'RINGS ON AFTER THE KEY' : 'STOPS WITH THE KEY') : ({ back: 'BACKWARDS ONLY', pnobk: 'PIANO, THEN BACKWARDS', echo: 'BACKWARDS ECHOES' })[k]) + soonNote(b)); }
  if (b.startsWith('del.')) { const k = b.slice(4);
    if (k === 'sync') put('del_sync', !tog('del_sync')); else { put('del_div', ['1/4', '1/8d', '1/8'].indexOf(k)); put('del_sync', 1); put('del_on', 1); }
    return flash('DELAY', fmt('del.time')); }
  /* the PAD's */
  let msg = '';
  if (g === 'pp') { const i = (cho('pad_preset') + (x === 'prev' ? -1 : 1) + PAD_NAMES.length) % PAD_NAMES.length, P = { ...HALO_DEF, ...HALO_PRESETS[PAD_NAMES[i]] };
    put('pad_preset', i);
    for (const k of ['swell', 'tone', 'width', 'amt', 'speed', 'space', 'tex', 'sub', 'air', 'spark']) put('pad_' + k, P[k]);
    for (const c of Object.keys(CHARS)) { put('pad_ch_' + c, P.ch.includes(c) ? 1 : 0); put('pad_lv_' + c, P.lv && P.lv[c] != null ? P.lv[c] : 1); }
    put('pad_motion', MOTIONS.indexOf(P.motion)); put('pad_voicing', VO.indexOf(P.voicing)); msg = PAD_NAMES[i].toUpperCase(); }
  else if (g === 'pc') { const lit = Object.keys(CHARS).filter(c => tog('pad_ch_' + c));
    if (lit.includes(x)) { if (lit.length === 1) return flash('PAD', 'ALWAYS ONE CHARACTER'); put('pad_ch_' + x, 0); }
    else { if (lit.length >= 3) put('pad_ch_' + lit[0], 0); put('pad_ch_' + x, 1); }
    msg = Object.keys(CHARS).filter(c => tog('pad_ch_' + c)).map(c => CHARS[c]).join(' + '); }
  else if (g === 'pm') { put('pad_motion', MOTIONS.indexOf(x)); msg = 'MOTION: ' + x.toUpperCase(); }
  else if (g === 'pv') { put('pad_voicing', VO.indexOf(x)); msg = 'VOICING: ' + VO_NAME[x]; }
  else if (g === 'pr') { put('pad_range', x === 'left' ? 1 : 0); msg = x === 'left' ? 'PLAYED BY THE LEFT HAND (BELOW MIDDLE C)' : 'PLAYED BY ALL KEYS'; }
  else if (g === 'po') { put('pad_oct', +x + 1); msg = 'PAD OCTAVE ' + (+x > 0 ? '+' : '') + x; }
  else if (g === 'dk') { if (x === 'name') return; put('dr_key', (cho('dr_key') + (x === 'prev' ? 11 : 1)) % 12); msg = 'DRONE IN ' + NM[cho('dr_key')]; }
  else if (g === 'dr') { put('dr_' + x, !tog('dr_' + x)); msg = 'DRONE: ' + (['root', 'fifth', 'oct', 'sub'].filter(d => tog('dr_' + d)).map(d => d.toUpperCase()).join(' + ') || 'OFF'); }
  else return;
  flash(g === 'pp' ? 'PAD SOUND' : 'PAD', msg + soonNote(b));
}
function stomp(s) {
  if (s === 'freeze') { put('verb_freeze', !tog('verb_freeze')); if (tog('verb_freeze')) put('verb_on', 1); return flash('FREEZE', tog('verb_freeze') ? 'THE HALL HOLDS' : 'OFF'); }
  const id = { tube: 'tube_on', rev: 'rev_on', del: 'del_on', verb: 'verb_on', tape: 'tape_on', plate: 'plate_on', pad: 'pad_on' }[s];
  put(id, !tog(id));
  flash(({ tube: 'TUBE', rev: 'REVERSE', del: 'DELAY', verb: 'REVERB', tape: 'TAPE', plate: 'PLATE', pad: 'PAD' })[s], (tog(id) ? 'ON' : 'OFF') + soonNote('s:' + s));
}

/* ---- the hands on it: knobs and faders (drag, scroll, double-click), buttons, switches, the drawers ---- */
function turn(k, v) { const c = ctl(idOf(k)); c.set(clamp(v, 0, 1)); flash(KNAME[k] || k, fmt(k) + soonNote(k)); }
m.addEventListener('pointerdown', e => { if (e.button) return; const t = e.target, d = t.dataset || {};
  if (t === grip) { const x0 = e.screenX, y0 = e.screenY, w0 = innerWidth; e.preventDefault(); grip.setPointerCapture(e.pointerId);
    const mv = ev => sizeTo(w0 + Math.max(ev.screenX - x0, (ev.screenY - y0) * 16 / 9), true),
      up = () => { grip.removeEventListener('pointermove', mv); grip.removeEventListener('pointerup', up); };
    grip.addEventListener('pointermove', mv); grip.addEventListener('pointerup', up); return; }
  if (d.k) { const k = d.k, y0 = e.clientY, u0 = +get(k) || 0, fd = t.closest('.fd'), c = ctl(idOf(k));
    const h = fd ? fd.querySelector('.track').getBoundingClientRect().height * .67 : t.closest('.kn').querySelector('.cap').getBoundingClientRect().height * 3.5;
    e.preventDefault(); c.begin();
    const mv = ev => turn(k, u0 + (y0 - ev.clientY) * (ev.shiftKey ? .25 : 1) / h), up = () => { removeEventListener('pointermove', mv); removeEventListener('pointerup', up); c.end(); };
    addEventListener('pointermove', mv); addEventListener('pointerup', up); return; }
  if (d.b || d.m || d.s) { e.preventDefault(); t.classList.add('down');
    const up = () => { removeEventListener('pointerup', up); t.classList.remove('down'); if (d.b) button(d.b); else if (d.m) openDrawer(d.m); else stomp(d.s); };
    addEventListener('pointerup', up); return; }
  if (view === 'stage' || open) { if (!t.closest('.glass')) { open = ''; view = 'simple'; layout(); } }   /* a click on the sky puts the glass away */
  else if (!IN_PLUGIN && t === skyc) pretendNote(); });
m.addEventListener('wheel', e => { const d = e.target.dataset || {}; if (!d.k) return; e.preventDefault(); const c = ctl(idOf(d.k)); c.begin(); turn(d.k, get(d.k) - Math.sign(e.deltaY) * .03); c.end(); }, { passive: false });
m.addEventListener('dblclick', e => { const d = e.target.dataset || {}; if (d.k && d.k in DEFK) { const c = ctl(idOf(d.k)); c.begin(); turn(d.k, DEFK[d.k]); c.end(); } });

/* ---- the face fills the window: laid out 1400 x 788, scaled to fit (as HORIZON's full screen) ---- */
function fit() { const f = Math.min(innerWidth / 1400, innerHeight / 788); ui.style.transform = `translateX(-50%) scale(${f.toFixed(4)})`; }
/* the window's size, from the face: − + in the corner, or the grip at the bottom right (Live on the Mac gives a plugin's window
   no edge to drag). One ask at a time while dragging; the last width wins. */
const grip = document.createElement('div'); grip.className = 'grip'; grip.title = 'drag to make the window bigger or smaller'; m.appendChild(grip);
let sizing = false, sizeNext = 0;
function sizeTo(w, quiet) { w = Math.round(w);
  if (!IN_PLUGIN) return quiet || flash('WINDOW', 'BIGGER AND SMALLER IN THE PLUGIN');
  if (sizing) { sizeNext = w; return; }
  sizing = true; juce.getNativeFunction('size')(w).then(got => { sizing = false; if (!quiet) flash('WINDOW', got + ' × ' + Math.round(got * 9 / 16));
    if (sizeNext) { const n = sizeNext; sizeNext = 0; sizeTo(n, quiet); } }, () => { sizing = false; }); }
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
function startSky() {
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
  if (e.status != null && String(e.status) !== status) { status = String(e.status); show(); }
  if (e.preset != null && (e.preset !== preset.name || e.pi !== preset.i)) { preset = { name: String(e.preset), i: +e.pi || 0, n: +e.pn || 0 }; show(); }
}
function pretendNote() { pretend = Math.min(.6, pretend + .12); onSky({ level: 0, hit: 100 }); }
if (IN_PLUGIN) window.__JUCE__.backend.addEventListener('sky', onSky);
layout();
