/* HORIZON's PAD: HALO (mixtape2\Devices\halo.js) in plain C++; see HaloPad.h. */
#include "HaloPad.h"
#include "Convolver.h"

namespace horizon {

static inline double mtof (double n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }
static inline float attackOf (float v) { return .03f * std::pow (200.0f, v) + 55 * std::pow (v, 8.0f); }   // SWELL in: 30 ms … 1 s at the middle … a minute
static inline float releaseOf (float v) { return .4f + 6.5f * v + 23 * std::pow (v, 6.0f); }              // and out: 0.4 s … 30 s
static inline float padCut (float tone) { return clampf (250 * std::pow (2.0f, tone * 6.2f), 80, 18000); }
static inline float periodOf (float v) { return 3 * std::pow (100.0f, 1 - v); }                              // SPEED: one motion cycle, 3 s … 5 min
static const int kBases[5] = { 36, 48, 60, 72, 84 };

/* ================= the characters: harmonics (or partials), how wide each is spread, a vowel ================= */
struct Char { int partsN; const float (*parts)[2]; int kind; float bw, bs, cap; int form; };
static const float kGlass[][2] = { { 1, 1 }, { 2, .35f }, { 3, .12f }, { 5.04f, .06f }, { 7.1f, .025f } };
static const float kOrgan[][2] = { { 1, 1 }, { 2, .8f }, { 3, .5f }, { 4, .45f }, { 6, .25f }, { 8, .22f }, { 10, .08f } };
static const float kBowl[][2] = { { 1, 1 }, { 2.71f, .55f }, { 5.15f, .32f }, { 8.43f, .16f }, { 12.4f, .06f } };
/* kind: how loud harmonic h is when there is no list of partials (0 warm 1/h, 2 choir, 3 strings, 4 air, 7 shruti) */
static const Char kCharDefs[8] = {
    { 0, nullptr, 0, 22, 1, .25f, 0 },  { 5, kGlass, -1, 7, .6f, .25f, 0 },  { 0, nullptr, 2, 55, 1, .25f, 1 }, { 0, nullptr, 3, 45, 1.1f, .25f, 0 },
    { 0, nullptr, 4, 380, 1.2f, 99, 0 }, { 7, kOrgan, -1, 3, .5f, .25f, 0 }, { 5, kBowl, -1, 4, .7f, .25f, 0 }, { 0, nullptr, 7, 12, 1, .25f, 2 } };
static float charAmp (int kind, int h)
{
    switch (kind)
    {
        case 0: return 1.0f / h;
        case 2: return 1 / std::pow ((float) h, .8f);
        case 3: return (h < 4 ? 1.0f : .85f) / h;
        case 4: return 1 / std::pow ((float) h, .6f);
        case 7: return (h % 2 ? 1.0f : .7f) / std::pow ((float) h, .55f);
        default: return 1.0f / h;
    }
}
static double formOf (int form, double f)
{
    static const double AH[3][2] = { { 730, 1 }, { 1090, .7 }, { 2440, .45 } }, NASAL[2][2] = { { 1150, 1 }, { 2600, .6 } };
    double g = .12;
    if (form == 1) for (auto& c : AH) { const double x = std::log2 (f / c[0]) / .28; g += c[1] * std::exp (-x * x); }
    if (form == 2) for (auto& c : NASAL) { const double x = std::log2 (f / c[0]) / .28; g += c[1] * std::exp (-x * x); }
    return g;
}

/* ================= the tables ================= */
PadTables::PadTables()
{
    for (auto& c : where) for (auto& w : c) w.store (-1);
    for (auto& c : asked) for (auto& a : c) a.store (false);
    for (int i = 0; i < kSlots; ++i) { refs[i].store (0); used[i].store (0); pinned[i].store (false); slotKey[i] = -1; }
    for (auto& x : q) x.store (0);
}

void PadTables::setRate (double r)
{
    if (r == rate) return;
    rate = r; gen.fetch_add (1);
    for (auto& c : where) for (auto& w : c) w.store (-1);
    for (auto& c : asked) for (auto& a : c) a.store (false);
}

void PadTables::ask (int ch, int note, bool pin)
{
    if (ch < 0 || ch >= kChars || note < 0 || note > 127) return;
    if (asked[ch][note].exchange (true)) return;
    const int h = qHead.load(), t = qTail.load (std::memory_order_acquire);
    if (h - t >= kQ) { asked[ch][note].store (false); return; }
    q[h % kQ].store ((ch * 128 + note) | (pin ? 1 << 16 : 0));
    qHead.store (h + 1, std::memory_order_release);
}

void PadTables::wantBases (int ch) { for (int b : kBases) ask (ch, b, true); }

int PadTables::take (int ch, int note)
{
    if (ch < 0 || ch >= kChars || note < 0 || note > 127) return -1;
    const int s = where[ch][note].load();
    if (s < 0) { ask (ch, note, false); return -1; }
    refs[s].fetch_add (1);
    if (where[ch][note].load() != s) { refs[s].fetch_sub (1); return -1; }   // being replaced this moment
    used[s].store (stamp.fetch_add (1));
    return s;
}

void PadTables::drop (int s) { if (s >= 0 && s < kSlots) refs[s].fetch_sub (1); }

bool PadTables::work()
{
    static thread_local int seenGen = -1;
    const int t = qTail.load(), h = qHead.load (std::memory_order_acquire);
    if (t == h) return false;                          // nothing asked for (and until the PAD is first used, no memory taken)
    if (! poolReady.load())
    {
        pool.reset (new float[(size_t) kSlots * TN]());
        poolReady.store (true);
    }
    const int g = gen.load();
    if (g != seenGen) { seenGen = g; for (int i = 0; i < kSlots; ++i) { slotKey[i] = -1; pinned[i].store (false); } }
    const int item = q[t % kQ].load();
    qTail.store (t + 1, std::memory_order_release);
    const int key = item & 0xFFFF, ch = key / 128, note = key % 128; const bool pin = (item >> 16) & 1;
    if (where[ch][note].load() >= 0) return true;
    /* a free slot, or the one used longest ago that no note holds and is not an octave table */
    int slot = -1;
    for (int i = 0; i < kSlots && slot < 0; ++i) if (slotKey[i] < 0) slot = i;
    for (int tries = 0; slot < 0 && tries < 4; ++tries)
    {
        uint32_t best = 0xFFFFFFFFu; int cand = -1;
        for (int i = 0; i < kSlots; ++i)
            if (! pinned[i].load() && refs[i].load() == 0 && used[i].load() < best) { best = used[i].load(); cand = i; }
        if (cand < 0) break;
        const int old = slotKey[cand];
        if (old >= 0) where[old / 128][old % 128].store (-1);
        if (refs[cand].load() != 0) { if (old >= 0) where[old / 128][old % 128].store (cand); continue; }   // a note took it just now
        if (old >= 0) asked[old / 128][old % 128].store (false);
        slotKey[cand] = -1; slot = cand;
    }
    if (slot < 0) { asked[ch][note].store (false); return true; }
    make (ch, note, pool.get() + (size_t) slot * TN);
    if (gen.load() != g) return true;                  // the rate changed meanwhile: thrown away
    slotKey[slot] = key; pinned[slot].store (pin); used[slot].store (stamp.fetch_add (1));
    where[ch][note].store (slot);
    return true;
}

/* PADsynth (Nasca O. Paul): every harmonic a Gaussian band in a 2^17-point spectrum, random phases (the same seed as HALO's),
   one inverse FFT: a loop with no seam, the ensemble's shimmer inside it (halo.js table) */
void PadTables::make (int ch, int note, float* out) const
{
    static thread_local FFT fft; static thread_local std::vector<float> re, im; static thread_local std::vector<double> amp;
    if (fft.size() != TN) { fft.setup (TN); re.assign (TN, 0); im.assign (TN, 0); amp.assign (TN / 2, 0); }
    std::fill (amp.begin(), amp.end(), 0.0); std::fill (re.begin(), re.end(), 0.0f); std::fill (im.begin(), im.end(), 0.0f);
    const Char& C = kCharDefs[ch];
    const double sr = rate, df = sr / TN, f0 = mtof (note), lim = .46 * sr;
    auto band = [&] (double h, double a) {
        const double f = f0 * h;
        if (f >= lim) return;
        const double g = a * (C.form ? formOf (C.form, f) : 1.0);
        const double bw = std::max (std::min ((std::pow (2.0, C.bw / 1200.0) - 1) * f0 * std::pow (h, (double) C.bs), C.cap * f0) / df, .6), c = f / df;
        for (int i = std::max (1, (int) std::floor (c - 3 * bw)), e = std::min (TN / 2 - 1, (int) std::ceil (c + 3 * bw)); i <= e; ++i)
        { const double x = (i - c) / bw; amp[(size_t) i] += g * std::exp (-x * x) / bw; }
    };
    if (C.parts) for (int p = 0; p < C.partsN; ++p) band (C.parts[p][0], C.parts[p][1]);
    else for (int h = 1, nh = std::max (1, (int) std::floor (lim / f0)); h <= nh; ++h) band (h, charAmp (C.kind, h));
    /* HALO's own random numbers (mulberry32, seeded by the note) */
    uint32_t s = (uint32_t) (note * 7 + 1);
    auto r = [&s]() { s += 0x6D2B79F5u; uint32_t t = (s ^ (s >> 15)) * (1u | s); t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t; return (double) (t ^ (t >> 14)) / 4294967296.0; };
    for (int i = 1; i < TN / 2; ++i)
    {
        if (amp[(size_t) i] == 0) continue;
        const double p = r() * 2 * kPi;
        re[(size_t) i] = (float) (amp[(size_t) i] * std::cos (p)); im[(size_t) i] = (float) (amp[(size_t) i] * std::sin (p));
    }
    fft.run (re.data(), im.data(), true);
    double e = 0; for (int i = 0; i < TN; ++i) e += (double) re[(size_t) i] * re[(size_t) i];
    const float k = (float) (.2 / std::sqrt (e / TN > 0 ? e / TN : 1));
    for (int i = 0; i < TN; ++i) out[i] = re[(size_t) i] * k;
}

/* ================= the pad ================= */
void HaloPad::prepare (double r, PadTables* t)
{
    rate = r; T = t;
    for (auto& v : voices) v.active = false;
    for (auto& b : bus) b.assign (64, 0.0f);
    for (auto* s : { &busG, &hissG, &droneG, &bfDet, &gateBase, &gateLfo, &preG, &postG, &wA, &wB, &dryG, &wetG, &panL, &panW, &volG, &shimG, &driftD })
        s->setup (rate, .05);
    busG.setup (rate, .25); bfDet.setup (rate, .5); panW.setup (rate, .5);
    busG.reset (.22f); gateBase.reset (1); droneG.reset (dlvOf (.6f)); postG.reset (4); preG.reset (.25f); volG.reset (P.vol * 2);
    vibL.setup ((int) (rate * .1)); vibR.setup ((int) (rate * .1));
    hissHp.set (Biquad::highpass, rate, 3500, 0);
    static const float DT[8] = { .0503f, .0631f, .0697f, .0743f, .0901f, .1019f, .1151f, .1247f };
    for (int i = 0; i < 8; ++i) { line[i].setup ((int) (rate * .2)); damp[i].set (Biquad::lowpass, rate, 6000, -6); lineGain[i] = .8f; (void) DT; }
    setParams (P, bpm);
}

float HaloPad::layerLevel (int ch) const
{
    int n = 0; for (bool b : P.ch) if (b) ++n;
    return P.lv[ch] / std::sqrt ((float) std::max (1, n));
}

void HaloPad::setParams (const Params& p, double b)
{
    P = p; if (b > 20) bpm = b;
    /* a character lit or put out while notes hold: the old fade out as the new fade in, over the swell time (morph) */
    bool changed = false; for (int i = 0; i < 8; ++i) if (P.ch[i] != chWas[i]) changed = true;
    if (changed)
    {
        const float f = clampf (attackOf (P.swell), .3f, 8);
        for (auto& v : voices)
        {
            if (! v.active || v.released) continue;
            for (auto& L : v.lay) if (L.ch >= 0 && ! P.ch[L.ch] && ! L.dying) { L.dying = true; L.target = 0; L.step = L.g / (float) (f * rate); }
            for (int c = 0; c < 8; ++c) if (P.ch[c]) { bool has = false; for (auto& L : v.lay) if (L.ch == c && ! L.dying) has = true; if (! has) addLayer (v, c, f); }
        }
        for (int i = 0; i < 8; ++i) chWas[i] = P.ch[i];
    }
    for (int c = 0; c < 8; ++c) if (P.ch[c]) T->wantBases (c);
    if (P.air > .01f) T->wantBases (4);
}

void HaloPad::startPart (Part& p, int ch, int note)
{
    p = Part{}; p.slot = T->take (ch, note);
    const double o = std::floor (rng.next() * PadTables::TN);
    if (p.slot >= 0) { p.xf = 1; p.pos[0] = o; p.pos[1] = std::fmod (o + PadTables::TN / 2, (double) PadTables::TN); return; }
    /* until its own is made: the nearest octave's at a speed near 1 */
    int b = kBases[0]; for (int x : kBases) if (std::abs (x - note) < std::abs (b - note)) b = x;
    p.base = T->take (ch, b); p.baseNote = b; p.step = mtof (note) / mtof (b);
    p.bpos[0] = o; p.bpos[1] = std::fmod (o + PadTables::TN / 2, (double) PadTables::TN);
}

void HaloPad::freePart (Part& p) { if (p.slot >= 0) T->drop (p.slot); if (p.base >= 0) T->drop (p.base); p.slot = p.base = -1; }

void HaloPad::addLayer (Voice& v, int ch, float fade)
{
    for (auto& L : v.lay)
        if (L.ch < 0)
        {
            L = Layer{}; L.ch = ch; L.target = layerLevel (ch);
            if (fade > 0) { L.g = 0; L.step = L.target / (float) (fade * rate); } else L.g = L.target;
            for (int k = 0; k < v.nNotes; ++k) startPart (L.part[k], ch, v.notes[k]);
            return;
        }
}

void HaloPad::noteOn (int n, int vel, bool drone)
{
    if (T == nullptr) return;
    for (auto& v : voices) if (v.active && v.n == n && ! v.released && v.drone == drone) releaseVoice (v);
    /* the audio stays easy: at most 10 held notes and 16 sounding (the drone's own not counted) */
    if (! drone)
    {
        int held = 0, alive = 0; Voice* oldestHeld = nullptr; Voice* victim = nullptr;
        for (auto& v : voices) if (v.active && ! v.drone && ! v.killed)
        {
            ++alive;
            if (! v.released) { ++held; if (! oldestHeld || v.age < oldestHeld->age) oldestHeld = &v; }
            else if (! victim) victim = &v;
        }
        if (held >= 10 && oldestHeld) killVoice (*oldestHeld, .03f);
        else if (alive >= 16) { if (! victim) for (auto& v : voices) if (v.active && ! v.drone && (! victim || v.age < victim->age)) victim = &v; if (victim) killVoice (*victim, .03f); }
    }
    Voice* v = nullptr;
    for (auto& x : voices) if (! x.active) { v = &x; break; }
    if (! v) for (auto& x : voices) if (! v || x.age < v->age) v = &x;
    for (auto& L : v->lay) for (auto& p : L.part) freePart (p);
    freePart (v->air); freePart (v->spark);
    *v = Voice{};
    v->active = true; v->drone = drone; v->n = n; v->age = ++clock;
    /* the VOICING: the note, and the notes it adds (not for the drone) */
    static const int vAdd[5][3][2] = { { { 0, 0 } }, { { 12, 55 } }, { { 7, 60 } }, { { -12, 50 }, { 7, 45 }, { 12, 40 } }, { { 5, 50 }, { 7, 50 } } };
    static const int vCount[5] = { 0, 1, 1, 3, 2 };
    v->notes[0] = n; v->noteLv[0] = 1; v->nNotes = 1;
    if (! drone)
        for (int k = 0; k < vCount[std::max (0, std::min (4, P.voicing))]; ++k)
        {
            const int m = n + vAdd[P.voicing][k][0];
            if (m >= 12 && m <= 120) { v->notes[v->nNotes] = m; v->noteLv[v->nNotes] = vAdd[P.voicing][k][1] / 100.0f; ++v->nNotes; }
        }
    for (int c = 0; c < 8; ++c) if (P.ch[c]) addLayer (*v, c, 0);
    if (P.sub > .01f) { v->subStep = mtof (n) / 2 / rate; v->subG = P.sub * .5f; }
    if (P.air > .01f) { startPart (v->air, 4, n); v->airG = P.air * .9f; }
    int first = 0; for (int c = 0; c < 8; ++c) if (P.ch[c]) { first = c; break; }
    v->sparkCh = first;
    if (P.spark > .01f || P.motion == 2) { if (n + 24 <= 120) startPart (v->spark, first, n + 24); v->sparkG = 1; }
    /* the swell: loudness and brightness bloom together */
    const float att = attackOf (P.swell);
    v->cut = padCut (P.tone); v->cutFrom = v->cut * .3f; v->fT = att * .8f; v->t = 0;
    v->peak = (.25f + .75f * std::pow (clampf ((float) vel, 1, 127) / 127, 1.2f)) / std::sqrt (std::max (1.0f, v->nNotes * .7f));
    v->vcaCoef = (float) (1 - std::exp (-1.0 / (att / 3 * rate)));
    v->f.set (Biquad::lowpass, rate, v->cutFrom, .6f);
}

void HaloPad::releaseVoice (Voice& v)
{
    if (v.released) return;
    v.released = true; v.peak = 0;
    const float R = releaseOf (P.swell);
    v.vcaCoef = (float) (1 - std::exp (-1.0 / (R / 4.5 * rate)));
    v.endAt = v.t + R * 1.5 + .1;
}

void HaloPad::killVoice (Voice& v, float tau)
{
    v.released = v.killed = true; v.peak = 0;
    v.vcaCoef = (float) (1 - std::exp (-1.0 / (tau * rate)));
    v.endAt = v.t + .15;
}

void HaloPad::noteOff (int n, bool drone)
{
    for (auto& v : voices) if (v.active && v.n == n && v.drone == drone && ! v.released) releaseVoice (v);
}

void HaloPad::killAll() { for (auto& v : voices) if (v.active) killVoice (v, .03f); }

bool HaloPad::busy() const { for (auto& v : voices) if (v.active) return true; return false; }

/* one side of a part: its own table at speed 1, or the octave's at a speed near 1 crossfading over to its own once made */
inline float HaloPad::readPart (const Part& p, int side) const
{
    const int TN = PadTables::TN;
    float y = 0;
    const float own = p.slot >= 0 ? p.xf : 0.0f;
    if (p.slot >= 0) y = T->data (p.slot)[(int) p.pos[side]] * own;
    if (p.base >= 0 && own < 1)
    {
        const float* b = T->data (p.base);
        const int i = (int) p.bpos[side]; const float f = (float) (p.bpos[side] - i);
        y += (b[i] * (1 - f) + b[(i + 1) % TN] * f) * (1 - own);
    }
    return y;
}

void HaloPad::render (float* outL, float* outR, int total)
{
    if (T == nullptr) return;
    const int TN = PadTables::TN;
    const double dt = 1.0 / rate;
    const float twoPi = 2 * (float) kPi;
    for (int done = 0; done < total;)
    {
        const int n = std::min (32, total - done);
        /* ---- once a block: the motions, the gains, the voices' own state ---- */
        const int m = P.motion; const float a = P.amt, f = 1 / periodOf (P.speed), cut = padCut (P.tone);
        int live = 0; for (auto& v : voices) if (v.active && ! v.released) ++live;
        busG.target = .22f / std::sqrt (std::max (1.0f, live / 3.0f));
        hissG.setup (rate, live ? .3 : releaseOf (P.swell) / 4); hissG.target = live ? P.tex * .05f * std::min (1.0f, live * .5f) : 0.0f;
        const float d = std::round ((1 + P.tex * 4) * 20) / 20; drive = d;
        postG.target = 4 / (1 + P.tex * .6f);
        const float w = .25f + .75f * P.width; wA.target = (1 + w) / 2; wB.target = (1 - w) / 2;
        const float sp = P.space * .72f; dryG.target = std::cos (sp * (float) kPi / 2); wetG.target = std::sin (sp * (float) kPi / 2) * 1.1f;
        const float Tr = 1.5f + P.space * 10;
        static const float DT[8] = { .0503f, .0631f, .0697f, .0743f, .0901f, .1019f, .1151f, .1247f };
        for (int i = 0; i < 8; ++i) { lineGain[i] = std::pow (10.0f, -3 * DT[i] / Tr); damp[i].set (Biquad::lowpass, rate, 6500 - P.space * 2500, -6); }
        hlp.set (Biquad::lowpass, rate, cut, 0);
        gateBase.target = m == 4 ? 1 - a * .5f : 1.0f; gateLfo.target = m == 4 ? a * .5f : 0.0f;
        driftD.target = m == 3 ? std::min (.03f, (std::pow (2.0f, a * 16 / 1200) - 1) / (twoPi * std::max (.05f, f * .65f))) : 0.0f;
        panL.target = m == 3 ? a * .55f : 0.0f; shimG.target = m == 2 ? a * .05f : 0.0f;
        volG.target = P.vol * 2;
        nowT += n * dt;
        /* WANDER: every so often a new, slow, random brightness and place, a third of a SPEED cycle apart */
        if (m == 5)
        {
            if (nowT >= wanderAt)
            {
                const float per = periodOf (P.speed);
                wanderAt = nowT + (1 + P.speed * 30) / 3;
                bfDet.setup (rate, per / 6); bfDet.target = (rng.next() * 2 - 1) * a * 1600 - a * 400;
                panW.setup (rate, per / 5); panW.target = (rng.next() * 2 - 1) * a * .5f;
            }
        }
        else { bfDet.setup (rate, .5); bfDet.target = 0; panW.setup (rate, .5); panW.target = 0; wanderAt = 0; }
        for (auto& b : bus) std::fill (b.begin(), b.begin() + n, 0.0f);

        for (auto& v : voices)
        {
            if (! v.active) continue;
            /* parts whose own table is now made: fade over to it (0.25 s; 0.05 s if nothing played meanwhile) */
            auto upgrade = [&] (Part& p, int ch, int note) {
                if (p.slot >= 0 || note > 127) return;
                const int s = T->take (ch, note);
                if (s < 0) { if (p.base < 0) { p.base = T->take (ch, p.baseNote); } return; }
                p.slot = s; p.xf = 0;
                p.pos[0] = std::floor (rng.next() * TN); p.pos[1] = std::fmod (p.pos[0] + TN / 2, (double) TN);
            };
            for (auto& L : v.lay) if (L.ch >= 0) for (int k = 0; k < v.nNotes; ++k) upgrade (L.part[k], L.ch, v.notes[k]);
            if (v.airG > 0) upgrade (v.air, 4, v.n);
            if (v.sparkG > 0) upgrade (v.spark, v.sparkCh, v.n + 24);
            /* its filter: up from 30% of TONE over the swell, then following TONE */
            if (! v.released) v.cut = cut;
            const float fc = v.t < v.fT ? v.cutFrom + (v.cut - v.cutFrom) * (float) (v.t / v.fT) : v.cut;
            v.f.set (Biquad::lowpass, rate, fc, .6f);
            for (auto& L : v.lay) if (L.ch >= 0 && ! L.dying && ! v.released) L.target = layerLevel (L.ch);
            const float subG = v.subStep > 0 ? P.sub * .5f : 0.0f, airG = P.air * .9f;
            const float sparkBase = (P.spark * .08f + (m == 2 ? a * .07f : 0.0f));
            const float shimAmt = shimG.target;
            float* bl = v.drone ? bus[2].data() : bus[0].data();
            float* br = v.drone ? bus[3].data() : bus[1].data();
            for (int i = 0; i < n; ++i)
            {
                if (v.t >= v.endAt) { v.active = false; break; }
                float sl = 0, sr = 0;
                for (auto& L : v.lay)
                {
                    if (L.ch < 0) continue;
                    if (L.step > 0) { if (L.g < L.target) L.g = std::min (L.target, L.g + L.step); else if (L.g > L.target) L.g = std::max (L.target, L.g - L.step); if (L.g == L.target) L.step = 0; }
                    else L.g += (L.target - L.g) * .0005f;
                    if (L.dying && L.g <= 0) { for (auto& p : L.part) freePart (p); L.ch = -1; continue; }
                    for (int k = 0; k < v.nNotes; ++k)
                    {
                        Part& p = L.part[k];
                        const float gl = L.g * v.noteLv[k];
                        sl += readPart (p, 0) * gl; sr += readPart (p, 1) * gl;
                    }
                }
                if (subG > 0) { const float s = std::sin (twoPi * (float) v.subPh) * subG; sl += s; sr += s; v.subPh += v.subStep; if (v.subPh >= 1) v.subPh -= 1; }
                if (v.airG > 0) { sl += readPart (v.air, 0) * airG; sr += readPart (v.air, 1) * airG; }
                float yl = v.f.tick (sl, 0), yr = v.f.tick (sr, 1);
                if (v.sparkG > 0)
                {
                    const float g = sparkBase + shimAmt * std::sin (twoPi * (float) ph[3]);
                    yl += readPart (v.spark, 0) * g; yr += readPart (v.spark, 1) * g;
                }
                v.vca += (v.peak - v.vca) * v.vcaCoef;
                bl[i] += yl * v.vca; br[i] += yr * v.vca;
                /* on */
                auto step = [&] (Part& p) {
                    for (int s = 0; s < 2; ++s)
                    {
                        if (p.slot >= 0) { p.pos[s] += 1; if (p.pos[s] >= TN) p.pos[s] -= TN; }
                        if (p.base >= 0) { p.bpos[s] += p.step; if (p.bpos[s] >= TN) p.bpos[s] -= TN; }
                    }
                    if (p.slot >= 0 && p.xf < 1) { p.xf = std::min (1.0f, p.xf + (float) (dt / (p.base >= 0 ? .25 : .05))); if (p.xf >= 1 && p.base >= 0) { T->drop (p.base); p.base = -1; } }
                };
                for (auto& L : v.lay) if (L.ch >= 0) for (int k = 0; k < v.nNotes; ++k) step (L.part[k]);
                if (v.airG > 0) step (v.air);
                if (v.sparkG > 0) step (v.spark);
                v.t += dt;
                if (v.released && v.vca < 1e-5f && v.t > .05) { v.active = false; break; }
            }
            if (! v.active) { for (auto& L : v.lay) for (auto& p : L.part) freePart (p); freePart (v.air); freePart (v.spark); }
        }

        /* ---- all notes together ---- */
        for (int i = 0; i < n; ++i)
        {
            ph[0] += f * dt; ph[1] += std::max (.05f, f * .65f) * dt; ph[2] += f * .43f * dt; ph[3] += 6.5 * dt; ph[4] += bpm / 60 * 2 * dt;
            for (auto& x : ph) if (x >= 1) x -= std::floor (x);
            const float bg = busG.next(), dg = droneG.next();
            /* TEXTURE's breath: noise through the notes' own TONE, while notes sound */
            const float hg = hissG.next(), hiss = hg > 1e-7f ? hlp.tick (hissHp.tick (rng.next() * 2 - 1)) * hg : 0.0f;
            float L = (bus[0][(size_t) i] + bus[2][(size_t) i] * dg) * bg + hiss, R = (bus[1][(size_t) i] + bus[3][(size_t) i] * dg) * bg + hiss;
            /* DRIFT: one slowly swinging delay bends the pitch of everything at once */
            const float vd = (float) ((.035 + driftD.next() * std::sin (twoPi * (float) ph[1])) * rate);
            vibL.push (L); vibR.push (R); L = vibL.read (vd); R = vibR.read (vd);
            /* BREATHE (and WANDER): the brightness swinging */
            const float det = (m == 1 ? a * 1800 * std::sin (twoPi * (float) ph[0]) : 0.0f) + bfDet.next();
            if ((i & 7) == 0) bf.set (Biquad::lowpass, rate, std::min (18000.0f, cut * 2.5f) * std::pow (2.0f, det / 1200), .5f);
            L = bf.tick (L, 0); R = bf.tick (R, 1);
            /* PULSE: eighth notes of the tempo */
            const float pw = std::sin (twoPi * (float) ph[4]) + .8f / 3 * std::sin (3 * twoPi * (float) ph[4]) + .5f / 5 * std::sin (5 * twoPi * (float) ph[4]);
            const float gate = gateBase.next() + gateLfo.next() * pw;
            L *= gate; R *= gate;
            /* TEXTURE's saturation: stacked chords bend, never clip */
            const float pre = preG.next(), post = postG.next(), td = std::tanh (drive);
            L = std::tanh (drive * 4 * clampf (L * pre, -1, 1)) / td * .25f * post;
            R = std::tanh (drive * 4 * clampf (R * pre, -1, 1)) / td * .25f * post;
            /* WIDTH */
            const float A = wA.next(), B = wB.next(), l2 = A * L + B * R, r2 = A * R + B * L; L = l2; R = r2;
            /* its own hall (SPACE): left into the even lines, right into the odd */
            float dsum = 0, d8[8], oL = 0, oR = 0;
            const float wob = .0007f * std::sin (twoPi * (float) (nowT * .23));
            for (int k = 0; k < 8; ++k)
            {
                const float dl = (float) ((DT[k] + (k == 0 || k == 5 ? wob : 0.0f)) * rate);
                d8[k] = damp[k].tick (line[k].read (dl)); dsum += d8[k];
                if (k % 2) oR += d8[k]; else oL += d8[k];
            }
            for (int k = 0; k < 8; ++k) line[k].push ((k % 2 ? R : L) + lineGain[k] * (d8[k] - .25f * dsum));
            const float dry = dryG.next(), wet = wetG.next();
            L = L * dry + oL * .3f * wet; R = R * dry + oR * .3f * wet;
            /* DRIFT's (and WANDER's) place: the browser's panner for a two-channel sound */
            const float pan = clampf (panL.next() * std::sin (twoPi * (float) ph[2]) + panW.next(), -1, 1), vol = volG.next();
            float pl, pr;
            if (pan <= 0) { const float x = (pan + 1) * (float) kPi / 2; pl = L + R * std::cos (x); pr = R * std::sin (x); }
            else { const float x = pan * (float) kPi / 2; pl = L * std::cos (x); pr = R + L * std::sin (x); }
            outL[done + i] += pl * vol; outR[done + i] += pr * vol;
        }
        done += n;
    }
}

} // namespace horizon
