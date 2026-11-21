/* HORIZON's sound engine: see HorizonEngine.h for what it is, and mixtape2\Devices\horizon.js (Engine, E.strike, E.apply) for the
   original each part follows. Times below are "key seconds" (time as heard, at the key's own pitch) unless they say otherwise. */
#include "HorizonEngine.h"

namespace horizon {

static inline float lerp (float a, float b, float f) { return a + (b - a) * f; }
/* a recording read between samples (cubic, as keyBuf does) */
static inline float readAt (const std::vector<float>& x, double p)
{
    const int n = (int) x.size() - 3;
    if (p < 0 || p >= n) return 0;
    const int j = (int) p; const float f = (float) (p - j);
    const float y0 = x[(size_t) (j > 0 ? j - 1 : 0)], y1 = x[(size_t) j], y2 = x[(size_t) j + 1], y3 = x[(size_t) j + 2];
    return y1 + 0.5f * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
}

/* =============================== the recordings =============================== */
void SampleSet::add (int note, int layerIndex, const float* L, const float* R, int n, double sr)
{
    if (note < 0 || note > 127 || layerIndex < 0 || layerIndex > 3 || L == nullptr || n <= 0) return;
    if (R == nullptr) R = L;
    float peak = 0;
    for (int i = 0; i < n; ++i) peak = std::max (peak, std::abs (L[i] + R[i]));
    int s0 = 0;
    while (s0 < n && std::abs (L[s0] + R[s0]) < peak * 0.01f) ++s0;
    s0 = std::max (0, s0 - (int) std::lround (sr * 0.002));                        // 2 ms before the hammer
    const double maxS = note < 48 ? 9.0 : note < 72 ? 6.5 : note < 88 ? 4.5 : 3.0;   // the lows ring longer
    const int len = std::min (n - s0, (int) std::lround (sr * maxS)), fade = (int) std::lround (sr * 0.5);
    if (len <= 4) return;
    Rec& r = recs[note][layerIndex];
    r.rate = sr;
    r.data.assign ((size_t) len + 3, 0.0f);                                        // (3 spare zeros for the cubic's look-ahead)
    for (int i = 0; i < len; ++i) r.data[(size_t) i] = (L[s0 + i] + R[s0 + i]) * 0.5f;
    for (int i = 0; i < fade && i < len; ++i) r.data[(size_t) (len - 1 - i)] *= (float) i / (float) fade;
    /* its loudness every 5 ms, and the strike's and the tail's (as horizon.js meas) */
    const int step = std::max (1, (int) std::lround (sr * kEnvStep)), nb = (len + step - 1) / step;
    r.env.assign ((size_t) nb, 0.0f);
    for (int b = 0; b < nb; ++b)
    {
        double s = 0; int c = 0;
        for (int i = b * step; i < std::min (len, (b + 1) * step); ++i) { s += (double) r.data[(size_t) i] * r.data[(size_t) i]; ++c; }
        r.env[(size_t) b] = (float) (s / std::max (1, c));
    }
    auto rms = [&] (double a, double b) {
        const int i0 = (int) std::lround (a * sr), i1 = std::min (len, (int) std::lround (b * sr));
        double s = 0; for (int i = i0; i < i1; ++i) s += (double) r.data[(size_t) i] * r.data[(size_t) i];
        return (float) std::sqrt (s / std::max (1, i1 - i0)) + 1e-7f;
    };
    r.strike = rms (0, .04); r.tail = rms (.25, .75);
}

float SampleSet::Rec::envAt (double t, double w) const
{
    if (env.empty()) return 0;
    const int nb = (int) env.size();
    int a = (int) std::floor ((t - w / 2) / kEnvStep), z = (int) std::ceil ((t + w / 2) / kEnvStep);
    a = std::max (0, std::min (nb - 1, a)); z = std::max (a + 1, std::min (nb, z));
    double s = 0; for (int i = a; i < z; ++i) s += env[(size_t) i];
    return (float) std::sqrt (s / (z - a));
}

/* two recordings of one key, lined up before they are mixed (as lagOf): each take's start was found by its loudness, which can be a
   millisecond out, enough for the low strings to cancel where they overlap; the lag (±3 ms) matching them best between 0.1 and 0.4 s */
void SampleSet::finish()
{
    for (int k = 0; k < 128; ++k)
        for (int a = 0; a < 4; ++a)
            for (int b = 0; b < 4; ++b)
            {
                lag[k][a][b] = 0;
                if (a == b || ! has (k, a) || ! has (k, b)) continue;
                if (b < a) { lag[k][a][b] = (int8_t) -lag[k][b][a]; continue; }
                const Rec &x = recs[k][a], &y = recs[k][b];
                const double sr = x.rate;
                const int i0 = (int) (.1 * sr), i1 = std::min ({ (int) (.4 * sr), (int) x.data.size() - 3, (int) y.data.size() - 203 }), M = (int) std::lround (.003 * sr);
                double best = -1e30; int d0 = 0;
                for (int d = -M; d <= M; ++d)
                {
                    double s = 0;
                    for (int i = i0; i < i1; i += 2) { const int j = i + d; if (j >= 0 && j < (int) y.data.size()) s += (double) x.data[(size_t) i] * y.data[(size_t) j]; }
                    if (s > best) { best = s; d0 = d; }
                }
                lag[k][a][b] = (int8_t) std::max (-127, std::min (127, d0));
            }
}

int SampleSet::count() const
{
    int c = 0;
    for (auto& k : recs) for (auto& l : k) if (! l.data.empty()) ++c;
    return c;
}

int SampleSet::nearest (int n, int li, int& found) const
{
    static const int D[7] = { 0, 1, -1, 2, -2, 3, -3 };
    li = std::max (0, std::min (3, li));
    const int base = std::min (108, std::max (21, (int) std::floor ((n - 21) / 3.0 + 0.5) * 3 + 21));
    for (int d : D)
    {
        const int order[5] = { li, 0, 1, 2, 3 };
        for (int l : order) { const int k = base + d * 3; if (has (k, l)) { found = l; return k; } }
    }
    return -1;
}

/* =============================== the hall (Crazy Dream Lite) =============================== */
static const float kLens[8] = { 0.0371f, 0.0437f, 0.0533f, 0.0619f, 0.0713f, 0.0831f, 0.0977f, 0.1079f };
static const float kMean = (0.0371f + 0.0437f + 0.0533f + 0.0619f + 0.0713f + 0.0831f + 0.0977f + 0.1079f) / 8;

void Hall::prepare (double r)
{
    rate = r;
    pre.setup ((int) (rate * 1.0));
    for (auto& l : line) l.setup ((int) (rate * 0.6));
    const float df[5] = { 190, 430, 970, 2100, 4700 };
    for (int i = 0; i < 5; ++i) diff[i].set (Biquad::allpass, rate, df[i], 0.55f);
    highCut.set (Biquad::lowpass, rate, 7500, -3.5f);
    for (int i = 0; i < N; ++i) { damp[i].set (Biquad::lowpass, rate, 8250, -3.5f); peak[i].set (Biquad::peaking, rate, 650 * std::pow (1.37f, (float) i), 8, 0); }
    for (auto& s : lineT) s.setup (rate, .04);
    gain.setup (rate, .04); sumG.setup (rate, .04); preT.setup (rate, .04); inGate.setup (rate, .04);
    inGate.reset (1);
    set (.74f, decOf (.6f), .024f, 40 * std::pow (15.0f, .35f), 0, false);
    for (int i = 0; i < N; ++i) lineT[i].reset (lineT[i].target);
    gain.reset (gain.target); sumG.reset (sumG.target); preT.reset (preT.target);
    /* BLOOM's convolvers: two slots (the new swell is made in the other while the old one plays), up to 12 s of ring */
    const int B = rate > 50000 ? 4096 : 2048;
    for (auto& s : sw) { for (auto& c : s.c) c.setup (B, (int) (12 * rate) + B); s.ready = false; }
    piece.assign ((size_t) B, 0.0f);
    swNow = swMaking = -1; swAmt = swLen = 0; swWantAmt = swWantLen = -1; swLive = false;
    swWet.setup (rate, .04); fdnG.setup (rate, .04); swWet.reset (0); fdnG.reset (1);
    clear();
}

/* BLOOM: Crazy Dream Lite's swell(): a convolver with a rising tail (noise on a rising curve, renderSwell), made only once BLOOM
   is turned up; its length follows the hall's decay. The new tail is made a piece each block in the other slot, then swapped in */
void Hall::bloom (float amt, float decay, double now)
{
    bloomAmt = amt;
    if (amt <= 0)
    {
        swWet.target = 0; fdnG.target = 1;
        if (swLive && now - swQuietSince > swLen + .5) { swLive = false; if (swNow >= 0) for (auto& c : sw[swNow].c) c.clear(); }
        return;
    }
    swQuietSince = now;
    const float len = std::min (12.0f, std::max (.4f, decay * (.25f + .5f * amt)));
    const float a = std::round (amt * 100) / 100, l = std::round (len * 100) / 100;
    if ((a != swAmt || l != swLen) && swMaking < 0)
    {
        if (a != swWantAmt || l != swWantLen) { swWantAmt = a; swWantLen = l; swWantSince = now; }
        if (swNow < 0 || now - swWantSince >= .14)
        {
            swMaking = swNow == 0 ? 1 : 0; swAmt = a; swLen = l;
            Swell& s = sw[swMaking]; s.ready = false; for (auto& c : s.c) { c.beginRing(); c.clear(); }
            sg.len = (int) std::lround (l * rate); sg.made = 0; sg.fo = (int) std::lround (rate * .03); sg.e = 0; sg.y[0] = sg.y[1] = 0;
            sg.k = (float) std::exp (-2 * kPi * std::min (7500.0, 16000.0) / rate); sg.pw = 1 + 2.5f * a;
        }
    }
    if (swMaking >= 0)
    {
        Swell& s = sw[swMaking];
        const int B = s.c[0].pieceSize();
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < B; ++i)
            {
                const int j = sg.made + i;
                if (j >= sg.len) { piece[(size_t) i] = 0; continue; }
                sg.y[ch] = (1 - sg.k) * (sg.rng.next() * 2 - 1) + sg.k * sg.y[ch];
                float e = std::pow ((float) j / (float) sg.len, sg.pw);
                if (j > sg.len - sg.fo) e *= (float) (sg.len - j) / (float) sg.fo;
                piece[(size_t) i] = sg.y[ch] * e; sg.e += (double) piece[(size_t) i] * piece[(size_t) i];
            }
            s.c[ch].addPiece (piece.data());
        }
        sg.made += B;
        if (sg.made >= sg.len || s.c[0].full())
        {
            s.scale = (float) (2.2 / std::sqrt (sg.e / 2 + 1e-12)); s.ready = true;
            swNow = swMaking; swMaking = -1;
        }
    }
    swLive = swNow >= 0;
    swWet.target = swNow >= 0 ? amt * .9f : 0.0f; fdnG.target = 1 - amt;
}

void Hall::clear()
{
    pre.clear(); for (auto& l : line) l.clear();
    lowCut.clear(); highCut.clear(); for (auto& d : diff) d.clear(); for (auto& d : damp) d.clear(); for (auto& p : peak) p.clear();
    for (auto& v : last) v = 0;
}

void Hall::set (float size, float decay, float predelay, float lc, float resonance, bool frz)
{
    const float scale = .35f + 3.4f * size;
    float g = std::pow (10.0f, -3 * kMean * scale / std::max (.3f, decay));
    if (frz) g = .9997f;
    g = std::min (g, .9997f);
    for (int i = 0; i < N; ++i) lineT[i].target = (float) (kLens[i] * scale * rate);
    gain.target = g; sumG.target = -2 * g / N;
    preT.target = (float) (std::min (.99f, std::max (0.0f, predelay)) * rate);
    inGate.target = frz ? 0.0f : 1.0f;
    lowCut.set (Biquad::highpass, rate, lc, -3.5f);
    for (int i = 0; i < N; ++i) damp[i].set (Biquad::lowpass, rate, frz ? 20000.0f : 8250.0f, -3.5f);
    resDb = 14 * resonance;
    for (int i = 0; i < N; ++i) peak[i].set (Biquad::peaking, rate, 650 * std::pow (1.37f, (float) i), 8, resDb);
    const float mod = .3f;
    depth = (mod > .005f && ! frz) ? (float) (mod * .0035f * (.4f + size) * rate) : 0.0f;   // in samples
    frozen = frz;
}

void Hall::process (float inL, float inR, float& outL, float& outR)
{
    /* in: one channel (as the browser mixes stereo down: half of each), its gate (FREEZE shuts it), pre-delay, low and high cut, diffusion */
    float x = (inL + inR) * 0.5f * inGate.next();
    const float pd = preT.next();
    pre.push (x);
    x = pd >= 1 ? pre.read (pd) : x;
    x = highCut.tick (lowCut.tick (x));
    /* BLOOM's swell hears the sound here (after the low and high cut, before the diffusion) */
    float bl = 0, br = 0;
    const float swg = swWet.next(), fg = fdnG.next();
    if (swLive && swNow >= 0) { Swell& s = sw[swNow]; bl = s.c[0].tick (x) * s.scale * swg; br = s.c[1].tick (x) * s.scale * swg; }
    for (auto& d : diff) x = d.tick (x);
    /* the two drifts, each used both ways (as depths[] in crazy-dream-lite.js) */
    const float hz = lfoHz;
    ph[0] += (float) (hz * .83 / rate); if (ph[0] >= 1) ph[0] -= 1;
    ph[1] += (float) (hz * 1.21 / rate); if (ph[1] >= 1) ph[1] -= 1;
    const float lfo[2] = { std::sin (ph[0] * 2 * (float) kPi), std::sin (ph[1] * 2 * (float) kPi) };
    const float g = gain.next(), sg = sumG.next();
    float d[N], sum = 0, L = 0, R = 0;
    for (int i = 0; i < N; ++i)
    {
        const int j = i % 4;
        const float dep = (j & 2 ? -1.0f : 1.0f) * depth * (1 - .15f * j);
        const float t = lineT[i].next() + dep * lfo[j & 1];
        d[i] = damp[i].tick (line[i].read (std::max (1.0f, t)));
        sum += d[i];
        if (i % 2) R += d[i]; else L += d[i];
    }
    for (int i = 0; i < N; ++i) line[i].push (x + g * d[i] + sg * sum);
    const float tap = .42f;
    if (resDb > .01f)
    {
        float cl = L, cr = R;
        for (int i = 0; i < N; i += 2) cl = peak[i].tick (cl, 0);
        for (int i = 1; i < N; i += 2) cr = peak[i].tick (cr, 1);
        outL = tap * .5f * (L + cl); outR = tap * .5f * (R + cr);
    }
    else { outL = tap * L; outR = tap * R; }
    outL = outL * fg + bl; outR = outR * fg + br;
}

/* =============================== the engine =============================== */
void Engine::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    now = 0;
    for (auto& v : voices) v.active = false;
    for (auto& r : revs) r.active = false;
    for (auto* b : { busS, busT, busG, busR }) for (int c = 0; c < 2; ++c) b[c].assign (64, 0.0f);
    /* the PLATE's convolvers: two slots, up to 5 s of ring each; made now (allocating), filled only when the PLATE is switched on */
    const int B = rate > 50000 ? 2048 : 1024;
    for (auto& s : plates) { for (auto& c : s.c) c.setup (B, (int) (5 * rate) + B); s.live = false; s.T = -1; s.in.setup (rate, .03); s.in.reset (0); }
    piece.assign ((size_t) B, 0.0f);
    plNow = plMaking = -1; plWantT = -1;
    plPreL.setup ((int) (rate * .13)); plPreR.setup ((int) (rate * .13));
    /* the PAD */
    for (auto& b : padBuf) b.assign (64, 0.0f);
    for (auto& h : padHeld) h = -1;
    for (auto& d : droneOn) d = -1;
    padWasOn = false; padQuietSince = -100;
    if (padTables) { padTables->setRate (rate); pad.prepare (rate, padTables); }
    gPadRev.setup (rate, .05); dkP.setup (rate, .05);
    for (auto* s : { &gPlIn, &gPlOut, &gPlPre, &dkL }) s->setup (rate, .05);
    gPlIn.setup (rate, .02); gPlPre.setup (rate, .03);
    for (auto* s : { &gPiano, &gSpark, &gStrike, &gGate, &gDelIn, &gVerbIn, &gD, &gV, &gShim, &gTapeWet, &gWow, &gFlutter, &gHiss, &gChDry, &gChMake,
                     &gWA, &gWB, &gOut, &dkV, &dkD, &HS.through, &HS.pre, &HS.post, &HS.del, &HS.rev, &HT.through, &HT.pre, &HT.post, &HT.del, &HT.rev })
        s->setup (rate, .05);
    for (auto* s : { &gGate, &gDelIn, &gVerbIn, &gTapeWet, &HS.through, &HT.through }) s->setup (rate, .02);
    dDelayT.setup (rate, .08);
    dlL.setup ((int) (rate * 3)); dlR.setup ((int) (rate * 3));
    shimD.setup ((int) (rate * .2));
    tapeL.setup ((int) (rate * .05)); tapeR.setup ((int) (rate * .05));
    hall.prepare (rate);
    chComp.setup (rate); chComp.knee = 10; chComp.attack = .02f; chComp.release = .25f;
    limiter.setup (rate); limiter.threshold = -3; limiter.knee = 2; limiter.ratio = 20; limiter.attack = .002f; limiter.release = .15f; limiter.ready();
    duckEnv.set (Biquad::lowpass, rate, 3, .5f);
    hissHp.set (Biquad::highpass, rate, 3500, .5f);
    sparkHi.set (Biquad::highshelf, rate, 3200, 0, 6);
    shimHp.set (Biquad::highpass, rate, 500, .5f); shimLp.set (Biquad::lowpass, rate, 6500, .5f);
    update (0);
    for (auto* s : { &gPiano, &gSpark, &gStrike, &gGate, &gDelIn, &gVerbIn, &gD, &gV, &gShim, &gTapeWet, &gWow, &gFlutter, &gHiss, &gChDry, &gChMake,
                     &gWA, &gWB, &gOut, &dkV, &dkD, &dDelayT, &HS.through, &HS.pre, &HS.post, &HS.del, &HS.rev, &HT.through, &HT.pre, &HT.post, &HT.del, &HT.rev })
        s->reset (s->target);
    for (auto* s : { &gPlIn, &gPlOut, &gPlPre, &dkL }) s->reset (s->target);
    pedalOn = false;
    for (int i = 0; i < 128; ++i) down[i] = sus[i] = false;
}

/* the PLATE's ring (plateIR in horizon.js): dense from the first millisecond (a steel sheet, no early echoes), dying away over
   DECAY, its highs a little sooner and its lows a little later; left and right each their own, so it is wide; made at the level the
   dry sound has. Made a piece each block in the other slot when DECAY settles on a new value (a knob turning: one new ring when it
   rests), then the sound moves across to it and the old ring dies away untouched */
void Engine::plateMaintain()
{
    const auto& pl = S.plate;
    const float T = std::round (plDec (pl.decay) * 100) / 100;
    if (pl.on)
    {
        const float cur = plMaking >= 0 ? pg.T : plNow >= 0 ? plates[plNow].T : -1;
        if (T != cur && plMaking < 0)
        {
            if (T != plWantT) { plWantT = T; plWantSince = now; }
            if (plNow < 0 || now - plWantSince >= .14)
            {
                plMaking = plNow == 0 ? 1 : 0;
                PlateSlot& s = plates[plMaking];
                s.live = false; s.T = T; for (auto& c : s.c) { c.beginRing(); c.clear(); }
                const double sr = rate;
                pg = PlateGen{}; pg.T = T; pg.len = (int) std::lround (sr * std::min (5.0, T * 1.2 + .05));
                pg.aL = (float) std::exp (-2 * kPi * 700 / sr); pg.aH = (float) std::exp (-2 * kPi * 4500 / sr);
                pg.kL = (float) std::exp (-6.91 / (T * 1.15 * sr)); pg.kM = (float) std::exp (-6.91 / (T * sr)); pg.kH = (float) std::exp (-6.91 / (T * .6 * sr));
                pg.kA = (float) std::exp (-1 / (.003 * sr));
                for (int c = 0; c < 2; ++c) { pg.gL[c] = pg.gM[c] = pg.gH[c] = pg.ga[c] = 1; }
            }
        }
        if (plNow >= 0) plates[plNow].live = true;
    }
    if (plMaking >= 0)
    {
        PlateSlot& s = plates[plMaking];
        const int B = s.c[0].pieceSize();
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < B; ++i)
            {
                if (pg.made + i >= pg.len) { piece[(size_t) i] = 0; continue; }
                const float n = rng.next() * 2 - 1;
                pg.lo[c] = pg.lo[c] * pg.aL + n * (1 - pg.aL);
                const float r = n - pg.lo[c];
                pg.lh[c] = pg.lh[c] * pg.aH + r * (1 - pg.aH);
                const float d = (pg.lo[c] * pg.gL[c] + pg.lh[c] * pg.gM[c] + (r - pg.lh[c]) * pg.gH[c]) * (1 - pg.ga[c]);
                pg.gL[c] *= pg.kL; pg.gM[c] *= pg.kM; pg.gH[c] *= pg.kH; pg.ga[c] *= pg.kA;
                piece[(size_t) i] = d; pg.e += (double) d * d;
            }
            s.c[c].addPiece (piece.data());
        }
        pg.made += B;
        if (pg.made >= pg.len || s.c[0].full())
        {
            s.scale = (float) (.8 / std::sqrt (pg.e / 2 + 1e-12));
            s.live = true; s.in.reset (0); s.in.target = 1;
            if (plNow >= 0) { PlateSlot& o = plates[plNow]; o.in.target = 0; o.ringUntil = now + o.T * 1.2 + .3; }
            plNow = plMaking; plMaking = -1;
        }
    }
    /* an old ring that has died away, or the PLATE switched off and its ring gone: nothing runs */
    for (int k = 0; k < 2; ++k)
    {
        PlateSlot& s = plates[k];
        if (! s.live) continue;
        if (k != plNow && now > s.ringUntil) { s.live = false; for (auto& c : s.c) c.clear(); }
    }
    if (! pl.on && plNow >= 0)
    {
        PlateSlot& s = plates[plNow];
        if (gPlIn.value > 1e-4f) s.ringUntil = now + s.T * 1.2 + .3;
        else if (s.live && now > s.ringUntil) { s.live = false; for (auto& c : s.c) c.clear(); }
    }
}

/* every setting into the sound (E.apply), once a block; the gains glide there sample by sample */
void Engine::update (int)
{
    const Settings& P = S;
    const auto& sh = P.shape;
    const float lv = .9f * P.level * P.level * 1.6f;
    gPiano.target = lv * dbToGain (lvDb (sh.tail)); gSpark.target = lv; gStrike.target = lv * dbToGain (lvDb (sh.punch));
    const float cut = cutOf (P.damp);
    sDamp.set (Biquad::lowpass, rate, cut, .5f); tDamp.set (Biquad::lowpass, rate, cut, .5f);
    sBright.set (Biquad::highshelf, rate, 2500, 0, (sh.bright - .5f) * 24 + sh.hammer * 5);
    tWarm.set (Biquad::lowpass, rate, warmCut (sh.warmth), .5f);
    gGate.target = P.piano == 0 ? 1.0f : 0.0f;
    /* TUBE: drive into an uneven soft curve (even harmonics, as a valve), a warm tilt, LEVEL; how much of each half goes through it */
    const auto& tb = P.tube;
    const float drv = 1 + tb.drive * 14;
    tubeDrive = drv; tubeBias = .08f + tb.warmth * .22f;
    Half* hs[2] = { &HS, &HT };
    const float a[2] = { sh.tubeS, sh.tubeT }, dsend[2] = { sh.delS, sh.delT }, rsend[2] = { sh.revS, sh.revT };
    for (int k = 0; k < 2; ++k)
    {
        Half& h = *hs[k];
        h.through.target = tb.on ? a[k] : 0.0f;
        h.pre.target = drv * .5f; h.post.target = (.35f + tb.level * 1.3f) / std::sqrt (drv * .5f + .5f);
        h.low.set (Biquad::lowshelf, rate, 220, 0, tb.warmth * 6); h.high.set (Biquad::highshelf, rate, 4200, 0, -tb.warmth * 9);
        h.del.target = dsend[k]; h.rev.target = rsend[k];
    }
    /* DELAY */
    const auto& dv = P.del;
    const float dtime = dv.sync ? (float) (60.0 / tempo) * kDivBeats[std::max (0, std::min (2, dv.div))] : delT (dv.time);
    dDelayT.target = clampf (dtime, .02f, 2.9f) * (float) rate;
    fbL = fbR = dv.rep * .82f;
    const float tc = 900 * std::pow (14.0f, dv.tone);
    fl.set (Biquad::lowpass, rate, tc, .3f); fr.set (Biquad::lowpass, rate, tc, .3f); dTone.set (Biquad::lowpass, rate, tc * 1.6f, .4f);
    gD.target = dv.mix * 1.1f; gDelIn.target = dv.on ? 1.0f : 0.0f;
    /* REVERB */
    const auto& vb = P.verb;
    gV.target = vb.mix * 1.3f; gVerbIn.target = vb.on ? 1.0f : 0.0f; gShim.target = vb.shim * .55f;
    hall.set (.35f + vb.size * .65f, decOf (vb.size), vb.pre * .2f, 40 * std::pow (15.0f, vb.low), vb.shim * .25f, vb.freeze);
    dkV.target = vb.duck; dkD.target = dv.duck;
    /* TAPE */
    const auto& tp = P.tape;
    gTapeWet.target = tp.on ? 1.0f : 0.0f;
    gWow.target = tp.on ? tp.wobble * .0026f : 0.0f; gFlutter.target = tp.on ? tp.wobble * .00009f : 0.0f;
    gHiss.target = tp.on ? tp.hiss * tp.hiss * .012f : 0.0f;
    /* the CHANNEL */
    const auto& ch = P.chan;
    chLow.set (Biquad::highpass, rate, ch.low < .02f ? 20.0f : 30 * std::pow (12.0f, ch.low), .5f);
    chBody.set (Biquad::peaking, rate, 250, .8f, (ch.body - .5f) * 18);
    chPres.set (Biquad::peaking, rate, 3200, .9f, (ch.pres - .5f) * 18);
    chAir.set (Biquad::highshelf, rate, 10000, 0, (ch.air - .5f) * 18);
    chComp.threshold = -6 - 30 * ch.glue; chComp.ratio = 1.5f + 6 * ch.glue; chComp.ready();
    gChMake.target = ch.blend * dbToGain (ch.glue * 14); gChDry.target = 1 - ch.blend * .55f;
    const float wd = ch.mono ? 0 : ch.width * 1.5f;
    gWA.target = (1 + wd) / 2; gWB.target = (1 - wd) / 2;
    gOut.target = dbToGain (-30 + 36 * ch.out);
    /* REVERSE's notes: their own DAMP */
    rDamp.set (Biquad::lowpass, rate, cut, .5f);
    /* the PLATE, on the CHANNEL: its send cut below LOW CUT and above HIGH CUT, PRE-DELAY (less the convolver's own lateness), MIX, DUCK */
    const auto& pl = P.plate;
    plHp.set (Biquad::highpass, rate, plLow (pl.low), .6f); plLp.set (Biquad::lowpass, rate, plHigh (pl.high), .6f);
    gPlIn.target = pl.on ? 1.0f : 0.0f; gPlOut.target = pl.mix * 4; dkL.target = pl.duck;
    gPlPre.target = (float) std::max (0.0, pl.pre * .12 * rate - plates[0].c[0].pieceSize());
    /* the PAD: HALO's sound (its level is LEVEL), its LOW CUT, INTO HALL, DUCK; the DRONE's LEVEL */
    const auto& pd = P.pad;
    padLow.set (Biquad::highpass, rate, pd.low > .02f ? 30 * std::pow (13.0f, pd.low) : 20.0f, .5f);
    gPadRev.target = pd.rev * 1.2f; dkP.target = pd.duck;
    if (padTables && (pd.on || pd.root || pd.fifth || pd.octave || pd.sub || pad.busy()))   // (unused, the PAD asks for no tables)
    {
        HaloPad::Params hp;
        for (int i = 0; i < 8; ++i) { hp.ch[i] = pd.ch[i]; hp.lv[i] = pd.lv[i]; }
        bool any = false; for (bool b : hp.ch) any = any || b;
        if (! any) hp.ch[0] = true;                     // always one character
        hp.motion = pd.motion; hp.voicing = pd.voicing; hp.tone = pd.tone; hp.width = pd.width; hp.swell = pd.swell; hp.amt = pd.amt;
        hp.space = pd.space; hp.tex = pd.tex; hp.speed = pd.speed; hp.sub = pd.subLv; hp.air = pd.air; hp.spark = pd.spark; hp.vol = pd.level;
        pad.setParams (hp, tempo);
        pad.setDroneLevel (dlvOf (pd.dlevel));
    }
}

/* the PAD switched off lets its held notes go (the drone stays); the DRONE's notes follow its switches and its KEY */
void Engine::padSync()
{
    if (! padTables) return;
    if (! S.pad.on && padWasOn) for (auto& h : padHeld) if (h >= 0) { pad.noteOff (h, false); h = -1; }
    padWasOn = S.pad.on;
    static const int off[4] = { 0, 7, 12, -12 };
    const bool want[4] = { S.pad.root, S.pad.fifth, S.pad.octave, S.pad.sub };
    for (int k = 0; k < 4; ++k)
    {
        const int note = want[k] ? 48 + S.pad.key + off[k] : -1;
        if (note == droneOn[k]) continue;
        if (droneOn[k] >= 0) pad.noteOff (droneOn[k], true);
        if (note >= 0) pad.noteOn (note, 92, true);
        droneOn[k] = note;
    }
}

/* ---- the voices ---- */
Engine::Voice& Engine::freeVoice()
{
    for (auto& v : voices) if (! v.active) return v;
    Voice* best = nullptr;
    for (auto& v : voices) if (v.released && (best == nullptr || v.age < best->age)) best = &v;   // the oldest let-go one
    if (best == nullptr) for (auto& v : voices) if (best == nullptr || v.age < best->age) best = &v;
    return *best;
}

void Engine::release (Voice& v, float sec)
{
    if (v.released) return;
    v.released = true;
    v.relCoef = (float) std::exp (-1.0 / (std::max (.005f, sec / 4) * rate));      // setTargetAtTime(0, t, rel/4)
    v.stopAt = (float) v.t + sec * 1.6f + .05f;
}

void Engine::kill (Voice& v, float k)
{
    v.released = true;
    v.relCoef = (float) std::exp (-1.0 / (std::max (.002f, k) * rate));
    v.stopAt = std::min (v.stopAt, (float) v.t + k * 8);
}

void Engine::noteOn (int n, int vel)
{
    if (n < 0 || n > 127) return;
    if (vel <= 0) { noteOff (n); return; }
    down[n] = true; sus[n] = false;
    for (auto& v : voices) if (v.active && v.note == n && ! v.released) release (v, .08f);   // the string struck again
    /* the PAD, played by the same key (whatever the PIANO switch says) */
    if (padTables && S.pad.on && ! (S.pad.range == 1 && n >= 60))
    {
        if (padHeld[n] >= 0) pad.noteOff (padHeld[n], false);
        const int m = std::max (12, std::min (120, n + 12 * (S.pad.oct - 1)));
        pad.noteOn (m, std::min (127, vel), false); padHeld[n] = m;
    }
    if (S.piano == 2 || ! ready()) { hitVel.store (vel, std::memory_order_relaxed); hitCount.fetch_add (1, std::memory_order_relaxed); return; }
    const bool backOnly = S.rev.on && S.rev.kind == 0;                        // REVERSE BACK: the backwards note is the note
    if (! backOnly) { Voice& v = freeVoice(); start (v, n, std::min (127, vel)); }
    if (S.rev.on) reverseNote (n, std::min (127, vel));
    hitVel.store (vel, std::memory_order_relaxed); hitCount.fetch_add (1, std::memory_order_relaxed);
}

/* every voice of a key let go (the strike always; the backwards ones only when RINGS ON is off: E.noteOff) */
void Engine::letGo (int n)
{
    if (padHeld[n] >= 0) { pad.noteOff (padHeld[n], false); padHeld[n] = -1; }
    const float rel = relOf (S.release);
    for (auto& v : voices) if (v.active && v.note == n && ! v.released) release (v, rel);
    if (! S.rev.rings)
        for (auto& r : revs)
            if (r.active && r.note == n && ! r.released)
            {
                if (r.wait > 0) { r.active = false; continue; }                  // not begun: never begins
                const float k = std::max (rel, .25f);
                r.released = true; r.relCoef = (float) std::exp (-1.0 / (std::max (.005f, k / 4) * rate)); r.stopAt = (float) r.u + k * 1.6f + .05f;
            }
}

void Engine::noteOff (int n)
{
    if (n < 0 || n > 127) return;
    down[n] = false;
    if (pedalOn) { sus[n] = true; return; }
    letGo (n);
}

void Engine::pedal (bool on)
{
    pedalOn = on; pedalIsDown.store (on, std::memory_order_relaxed);
    if (on) return;
    for (int n = 0; n < 128; ++n)
        if (sus[n]) { sus[n] = false; if (! down[n]) letGo (n); }
}

/* REVERSE (E.noteOn): the key's recording backwards (its first seconds, ending on the hammer), swelling in over SWELL; BACK: that
   is the note (after TIME when in beats); P+B: the piano, then the backwards note TIME later; ECHO: backwards echoes TIME apart,
   left, right, left…, each REPEATS quieter */
void Engine::reverseNote (int n, int vel)
{
    const SampleSet& set = *samples.load (std::memory_order_acquire);
    const auto& rv = S.rev;
    int li = 0; while (li < 3 && vel >= kLayerTop[li]) ++li;
    const int lo = li ? kLayerTop[li - 1] : 1;
    const float frac = clampf ((float) (vel - lo) / (float) (kLayerTop[li] - lo), 0, 1), amp = (.62f + .38f * frac) * 1.15f;
    int lf = 0; const int rn = set.nearest (n, li, lf);
    if (rn < 0) return;
    const auto& rec = set.recs[rn][lf];
    const double pr = std::pow (2.0, (n - rn) / 12.0), dur = std::min (rec.seconds() / pr, 4.2);
    const float T = rv.sync ? (float) (60.0 / tempo) * kRevBeats[std::max (0, std::min (6, (int) std::lround (rv.time * 6)))] : freeT (rv.time);
    const float sw = (float) std::min ((double) swellOf (rv.swell), dur - .02), lv = amp * rv.mix * 1.3f;
    if (sw <= .01f) return;
    auto one = [&] (float when, float a, float pan) {
        RevVoice* v = nullptr;
        for (auto& r : revs) if (! r.active) { v = &r; break; }
        if (v == nullptr) for (auto& r : revs) if (v == nullptr || r.age < v->age) v = &r;   // the oldest gives way
        *v = RevVoice{};
        v->active = true; v->note = n; v->age = ++clock; v->rec = &rec; v->pr = pr; v->wait = when; v->sw = sw; v->amp = a; v->att = sw * .45f;
        panGains (pan, v->panL, v->panR);
    };
    const float pan = clampf ((n - 64) / 44.0f, -1, 1) * .45f;
    if (rv.kind == 0) one (rv.sync ? T : std::min (T, .05f), amp * (.4f + rv.mix * .9f), pan);
    else if (rv.kind == 1) one (T, lv, pan);
    else { float a = lv; for (int k = 1; k <= 8 && a > .015f; ++k) { one (T * k, a, k % 2 ? -.55f : .55f); a *= rv.rep * .95f; } }
}

void Engine::renderRev (RevVoice& v, int n)
{
    const double dt = 1.0 / rate;
    int i = 0;
    if (v.wait > 0) { const int w = (int) std::ceil (v.wait * rate); if (w >= n) { v.wait -= n * dt; return; } i = w; v.wait = 0; }
    const auto& x = v.rec->data; const double k = v.pr * v.rec->rate;
    for (; i < n; ++i)
    {
        const double kt = v.sw - v.u;                                            // the key's time, running backwards to the hammer
        if (kt <= 0 || v.u >= v.stopAt) { v.active = false; return; }
        const float g = (v.u < v.att ? v.amp * (float) v.u / v.att : v.amp) * v.relG;
        const float y = readAt (x, kt * k) * g;
        busR[0][(size_t) i] += y * v.panL; busR[1][(size_t) i] += y * v.panR;
        v.u += dt;
        if (v.released) { v.relG *= v.relCoef; if (v.relG < 1e-4f) { v.active = false; return; } }
    }
}

void Engine::allNotesOff()
{
    if (padTables) { pad.killAll(); for (auto& h : padHeld) h = -1; for (auto& d : droneOn) d = -1; }
    for (auto& r : revs) r.active = false;
    for (auto& v : voices) if (v.active) kill (v, .02f);
    pedalOn = false; pedalIsDown.store (false);
    for (int i = 0; i < 128; ++i) down[i] = sus[i] = false;
}

/* a struck key, as SOUND LAB shapes it (E.strike): the strike (its first LENGTH, from the HAMMER's harder recording if turned up,
   matched in level) crossfades into the tail (from SOFT TAIL's softer recording if turned up); SUSTAIN gives back part of the
   tail's dying away, ENDLESS hands it to a level loop; DARKEN closes a filter on the tail as it rings; SPARKLE adds glints */
void Engine::start (Voice& v, int n, int vel)
{
    const SampleSet& set = *samples.load (std::memory_order_acquire);
    const auto& sh = S.shape;
    const float h = sh.hammer;
    int li = 0; while (li < 3 && vel >= kLayerTop[li]) ++li;
    const int lo = li ? kLayerTop[li - 1] : 1;
    const float frac = clampf ((float) (vel - lo) / (float) (kLayerTop[li] - lo), 0, 1), amp = (.62f + .38f * frac) * 1.15f;
    const int ls = (int) std::lround (li + h * (3 - li)), lt = (int) std::lround (li - sh.soft * li);
    int lsF = 0, ltF = 0, l0F = 0;
    const int sN = set.nearest (n, ls, lsF);
    if (sN < 0) return;
    int tN = set.nearest (n, lt, ltF);
    if (tN < 0) { tN = sN; ltF = lsF; }
    const int n0 = set.nearest (n, li, l0F);
    const auto &rS = set.recs[sN][lsF], &rT = set.recs[tN][ltF];
    const SampleSet::Rec* r0 = n0 >= 0 ? &set.recs[n0][l0F] : nullptr;
    const bool two = ! (tN == sN && ltF == lsF);

    v = Voice{};
    v.active = true; v.note = n; v.age = ++clock; v.two = two;
    auto src = [&] (Src& s, const SampleSet::Rec& r, int recNote) { s.rec = &r; s.pr = std::pow (2.0, (n - recNote) / 12.0); s.step = s.pr * r.rate / rate; s.pos = 0; s.start = 0; };
    src (v.sS, rS, sN); src (v.sT, two ? rT : rS, two ? tN : sN);
    /* HAMMER hits harder: the harder recording with half its extra loudness kept, and up to 4 dB more (and a brighter strike: update) */
    v.aS = amp * (r0 ? std::pow (clampf (r0->strike / rS.strike, .2f, 5), 1 - .5f * h) : 1.0f) * dbToGain (4 * h);
    v.aT = amp * (r0 ? clampf (r0->tail / rT.tail, .2f, 5) : 1.0f);
    v.aH = amp * (r0 ? clampf (r0->tail / rS.tail, .2f, 5) : 1.0f);
    /* with two recordings the strike's one rings on into the tail for C before the tail's takes over, so a hard hit's ring is heard */
    v.L = lenOf (sh.len); v.X = std::max (.012f, v.L * .6f); v.C = two ? .03f + .22f * std::max (h, sh.soft * .5f) : 0.0f;
    v.att = attOf (S.damp); v.tEnd = v.L + v.X; v.tTail = v.tEnd + v.C;
    panGains (clampf ((n - 64) / 44.0f, -1, 1) * .45f, v.panL, v.panR);
    /* the two takes lined up (lagOf): > 0, the tail's is late, so it starts that far in */
    if (two && tN == sN)
    {
        const double d = set.lag[sN][lsF][ltF] / rT.rate / v.sT.pr;                // in key seconds
        v.sT.start = std::max (0.0, -d); v.sT.pos = std::max (0.0, d) * v.sT.pr * rT.rate;
    }
    /* DARKEN: the tail grows darker as it rings */
    v.darken = sh.darken > .02f;
    if (v.darken) { v.dkTarget = std::max (300.0f, 18000 * std::pow (.04f, sh.darken)); v.dk.set (Biquad::lowpass, rate, 18000, .4f); }
    /* ENDLESS (SUSTAIN at its top): a stretch of the tail (0.6 … 3.2 s) levelled and looped, in step with the recording */
    const double durT = rT.seconds() / v.sT.pr;
    if (sh.sustain >= .97f)
    {
        const float A = .6f, b = (float) std::min (durT - .6, 3.2), XF = .25f;
        if (b - A >= XF * 3)
        {
            v.endless = true; v.loopA = A; v.loopXF = XF; v.loopN = (b - A) - XF; v.loopAt = A + XF;
            v.t1 = std::max (v.loopAt, v.tTail);
            v.loopRef = rT.envAt ((A + XF + .15) * v.sT.pr, .1 * v.sT.pr);
        }
    }
    else if (sh.sustain > .01f)
    {
        v.susPow = std::pow (std::min (sh.sustain, .96f), 1.6f) * .95f;
        v.susRef = rT.envAt ((v.tTail + .003 + .05) * v.sT.pr, .1 * v.sT.pr);
    }
    /* SPARKLE (as the core-sample Sustainer's): short glints from the bright early ring of the key an octave or two higher, each
       levelled to the strike, scattered over the first seconds of the ring, quieter as it dies, each placed somewhere left to right */
    if (sh.sparkle > .01f)
    {
        const float ref = rS.envAt (.03 * v.sS.pr, .04 * v.sS.pr) * v.aS * sh.sparkle * .8f;
        const int cnt = (int) std::lround (3 + 22 * sh.sparkle);
        for (int k = 0; k < cnt && v.nGrains < kMaxGrains; ++k)
        {
            const int up = (n + 24 <= 108 && rng.next() < .5f) ? 24 : 12;
            if (n + up > 108) continue;
            int gl = 0; const int gN = set.nearest (n + up, lt, gl);
            if (gN < 0) continue;
            const auto& rG = set.recs[gN][gl];
            const double pr = std::pow (2.0, (n + up - gN) / 12.0), dur = rG.seconds() / pr;
            Grain& g = v.grains[v.nGrains];
            g.rec = &rG; g.step = pr * rG.rate / rate;
            g.when = .05f + std::pow (rng.next(), 1.5f) * 3; g.len = .025f + rng.next() * .06f;
            const float off = .03f + rng.next() * (float) std::min (.4, dur - .2);
            g.pos = off * pr * rG.rate;
            const float e = rG.envAt ((off + g.len / 2) * pr, g.len * pr);
            g.lv = std::min (40.0f, ref * std::exp (-g.when / 2.2f) * (.5f + rng.next() * .5f) / std::max (e, 1e-5f));
            panGains ((rng.next() * 2 - 1) * .8f, g.l, g.r);
            ++v.nGrains;
        }
    }
}

void Engine::renderVoice (Voice& v, int n)
{
    const double dt = 1.0 / rate;
    const auto& xS = v.sS.rec->data; const auto& xT = v.sT.rec->data;
    const double lenS = (double) xS.size() - 3, lenT = (double) xT.size() - 3;
    const float pi2 = (float) kPi / 2;
    /* once a block: SUSTAIN's lift, ENDLESS's levelling, DARKEN's filter */
    const float tau = (float) v.t;
    if (v.susPow > 0 && tau > v.tTail + .003f)
    {
        const float e = v.sT.rec->envAt (tau * v.sT.pr, .1 * v.sT.pr);
        v.lift = std::min (31.6f, std::pow (v.susRef / std::max (e, 1e-7f), v.susPow));
    }
    double qA = 0;
    if (v.endless && tau >= v.t1 - .05f)
    {
        qA = std::fmod (v.loopXF + (tau - v.loopAt), (double) v.loopN);
        if (qA < 0) qA += v.loopN;
        const auto& r = *v.sT.rec; const double pr = v.sT.pr;
        v.gLoopA = v.loopRef / std::max (1e-6f, r.envAt ((v.loopA + qA) * pr, .1 * pr));
        v.gLoopB = v.loopRef / std::max (1e-6f, r.envAt ((v.loopA + qA + v.loopN) * pr, .1 * pr));
        v.gLoopA = std::min (v.gLoopA, 30.0f); v.gLoopB = std::min (v.gLoopB, 30.0f);
    }
    if (v.darken && tau > v.tTail)
        v.dk.set (Biquad::lowpass, rate, 18000 * std::pow (v.dkTarget / 18000, std::min (1.0f, (tau - v.tTail) / 3.5f)), .4f);

    const float sEnd = std::max (v.att, v.L);
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) v.t;
        if (t >= v.stopAt) { v.active = false; return; }
        /* the strike's take, and the tail's */
        const bool sOn = ! (v.two && t > v.tTail + .05f);
        const float xs = sOn ? readAt (xS, v.sS.pos) : 0.0f;
        float xt = xs;
        if (v.two) xt = t >= v.sT.start ? readAt (xT, v.sT.pos) : 0.0f;
        /* the strike's gain: up over the attack, held to LENGTH, gone over X */
        float gs = t < v.att ? v.aS * t / v.att : t < sEnd ? v.aS : t < sEnd + v.X ? v.aS * (1 - (t - sEnd) / v.X) : 0.0f;
        /* the tail's, and (two takes) the strike's take ringing on into the tail, handed over with equal-power-ish curves */
        float gt, gh = 0;
        if (v.two)
        {
            if (t < v.tEnd) gt = 0;
            else if (t < v.tTail) gt = v.aT * std::pow (std::sin ((t - v.tEnd) / v.C * pi2), 1.5f);
            else gt = v.aT;
            if (t < v.L) gh = 0;
            else if (t < v.tEnd) gh = v.aH * (t - v.L) / (v.tEnd - v.L);
            else if (t < v.tTail) gh = v.aH * std::pow (std::cos ((t - v.tEnd) / v.C * pi2), 1.5f);
        }
        else gt = t < v.L ? 0.0f : t < v.tTail ? v.aT * (t - v.L) / (v.tTail - v.L) : v.aT;
        if (t >= v.tTail) gt *= v.lift;
        float tail = xt * gt + xs * gh;
        /* ENDLESS: the level loop fades in over 0.3 s as the recording fades out */
        if (v.endless && t >= v.t1)
        {
            const float f = std::min (1.0f, (t - v.t1) / .3f);
            tail = xt * gt * (1 - f);
            double q = std::fmod (v.loopXF + (t - v.loopAt), (double) v.loopN);
            const auto& r = *v.sT.rec; const double pr = v.sT.pr, k = pr * r.rate;
            float y = readAt (r.data, (v.loopA + q) * k) * v.gLoopA;
            if (q < v.loopXF)
            {
                const float w = (float) (q / v.loopXF) * pi2;
                y = y * std::sin (w) + readAt (r.data, (v.loopA + q + v.loopN) * k) * v.gLoopB * std::cos (w);
            }
            tail += y * v.aT * f;
        }
        if (v.darken) tail = v.dk.tick (tail);
        const float rg = v.relG;
        const float s = xs * gs * rg; tail *= rg;
        busS[0][(size_t) i] += s * v.panL; busS[1][(size_t) i] += s * v.panR;
        busT[0][(size_t) i] += tail * v.panL; busT[1][(size_t) i] += tail * v.panR;
        /* the glints */
        for (int g = 0; g < v.nGrains; ++g)
        {
            Grain& gr = v.grains[g];
            const float e = t - gr.when;
            if (e < 0 || e > gr.len) continue;
            const float a = e < .15f * gr.len ? gr.lv * e / (.15f * gr.len) : gr.lv * std::pow (.02f, (e - .15f * gr.len) / (.85f * gr.len));
            const float y = readAt (gr.rec->data, gr.pos) * a * rg;
            gr.pos += gr.step;
            busG[0][(size_t) i] += y * gr.l; busG[1][(size_t) i] += y * gr.r;
        }
        /* on */
        v.sS.pos += v.sS.step;
        if (v.two && t >= v.sT.start) v.sT.pos += v.sT.step;
        if (v.released) v.relG *= v.relCoef;
        v.t += dt;
        /* a voice ends when its recording does (an ENDLESS one only when let go) */
        const bool tailDone = v.two ? v.sT.pos >= lenT : v.sS.pos >= lenS;
        if (tailDone && t > v.tTail && ! v.endless) { v.active = false; return; }
        if (v.released && v.relG < 1e-4f) { v.active = false; return; }
    }
}

/* ---- the sound, a block at a time ---- */
void Engine::render (float* outL, float* outR, int total)
{
    const float twoPi = 2 * (float) kPi;
    double sq = 0;
    for (int done = 0; done < total;)
    {
        const int n = std::min (32, total - done);
        update (n);
        plateMaintain();
        hall.bloom (S.verb.bloom * .9f, decOf (S.verb.size), now);
        now += n / rate;
        for (auto* b : { busS, busT, busG, busR }) for (int c = 0; c < 2; ++c) std::fill (b[c].begin(), b[c].begin() + n, 0.0f);
        if (ready()) { for (auto& v : voices) if (v.active) renderVoice (v, n); for (auto& r : revs) if (r.active) renderRev (r, n); }
        const bool plateLive = plates[0].live || plates[1].live;
        /* the PAD: run while it sounds and for its hall's ring after (silent a while, it costs nothing) */
        for (auto& b : padBuf) std::fill (b.begin(), b.begin() + n, 0.0f);
        if (padTables)
        {
            padSync();
            if (pad.busy()) padQuietSince = now;
            if (now - padQuietSince < 14) pad.render (padBuf[0].data(), padBuf[1].data(), n);
        }

        for (int i = 0; i < n; ++i)
        {
            /* the strike: DAMP, BRIGHT, PUNCH; the tail: WARMTH, LEVEL, DAMP, and SPARKLE's glints joining after (they keep their glitter) */
            const float gSt = gStrike.next(), gPi = gPiano.next(), gSp = gSpark.next();
            float sx[2], tx[2];
            for (int c = 0; c < 2; ++c)
            {
                sx[c] = sBright.tick (sDamp.tick (busS[c][(size_t) i], c), c) * gSt;
                tx[c] = tDamp.tick (tWarm.tick (busT[c][(size_t) i], c) * gPi, c) + sparkHi.tick (busG[c][(size_t) i] * gSp, c);
            }
            /* each half through its TUBE (how much: its TUBE send), heard while PIANO is ON, into the DELAY and the REVERB by its sends */
            float o[2][2];
            Half* hs[2] = { &HS, &HT }; float* in[2] = { sx, tx };
            for (int k = 0; k < 2; ++k)
            {
                Half& h = *hs[k];
                const float th = h.through.next(), pre = h.pre.next(), post = h.post.next();
                for (int c = 0; c < 2; ++c)
                {
                    const float x = in[k][c];
                    float y = clampf (x * th * pre, -1, 1);
                    y = (std::tanh (2 * (y - tubeBias)) - std::tanh (-2 * tubeBias)) / std::tanh (2.0f);
                    y = h.high.tick (h.low.tick (y, c), c) * post;
                    o[k][c] = y + x * (1 - th);
                }
            }
            const float sD = HS.del.next(), tD = HT.del.next(), sR = HS.rev.next(), tR = HT.rev.next(), gate = gGate.next();
            /* REVERSE's backwards notes, through their DAMP: heard, and into the DELAY, the REVERB and the PLATE */
            const float rl = rDamp.tick (busR[0][(size_t) i], 0), rr = rDamp.tick (busR[1][(size_t) i], 1);
            float L = gate * (o[0][0] + o[1][0]) + rl, R = gate * (o[0][1] + o[1][1]) + rr;
            const float dIn = gDelIn.next();
            float delL = dIn * (o[0][0] * sD + o[1][0] * tD + rl), delR = dIn * (o[0][1] * sD + o[1][1] * tD + rr);
            float vL = o[0][0] * sR + o[1][0] * tR + rl, vR = o[0][1] * sR + o[1][1] * tR + rr;
            /* the PAD: heard as it is (FX ONLY never silences it), its LOW CUT and DUCK; into the hall by INTO HALL */
            const float padDk = 1 + dkP.next() * duckNow, gpr = gPadRev.next();
            const float ql = padLow.tick (padBuf[0][(size_t) i], 0) * padDk, qr = padLow.tick (padBuf[1][(size_t) i], 1) * padDk;
            L += ql; R += qr; vL += ql * gpr; vR += qr * gpr;
            /* the PLATE's send: the piano's two halves (heard or not), the backwards notes, the pad, the echoes (added below) */
            float pL = o[0][0] + o[1][0] + rl + ql, pR = o[0][1] + o[1][1] + rr + qr;
            /* DUCK: how loud the piano plays (both halves, heard or not), a slow envelope; a firm note ducks fully, a fading ring a little */
            {
                const float eL = duckEnv.tick (std::abs (o[0][0] + o[1][0]), 0), eR = duckEnv.tick (std::abs (o[0][1] + o[1][1]), 1);
                auto shape = [] (float x) { return -std::min (1.0f, std::max (0.0f, x) * 45); };
                duckNow = .5f * (shape (eL) + shape (eR));
            }
            /* DELAY: ping-pong; its input stops when switched off, so its echoes ring out */
            {
                const float dT = dDelayT.next(), m = dTone.tick ((delL + delR) * .5f);
                const float yl = dlL.read (dT), yr = dlR.read (dT);
                dlL.push (m + fbR * fr.tick (yr)); dlR.push (fbL * fl.tick (yl));
                const float gd = gD.next(), duck = 1 + dkD.next() * duckNow;
                L += yl * gd * duck; R += yr * gd * duck;
                vL += yl * gd; vR += yr * gd;
                pL += yl * gd; pR += yr * gd;
            }
            /* REVERB: the hall, its SHIMMER fed back into it, its DUCK */
            {
                const float gi = gVerbIn.next();
                float wl, wr;
                hall.process (vL * gi + shimOut, vR * gi + shimOut, wl, wr);
                /* the shimmer: an octave up (two read points sweeping through a 60 ms window, each faded round its jump) */
                const float Wn = .06f, x = (wl + wr) * .5f;
                shimPh += (float) (1.0 / (Wn * rate)); if (shimPh >= 1) shimPh -= 1;
                float y = 0;
                for (int k = 0; k < 2; ++k)
                {
                    float p = shimPh - .5f * k; if (p < 0) p += 1;
                    const float saw = 2 * (p - std::floor (p + .5f)), w = .5f + .5f * std::cos (twoPi * p);
                    y += shimD.read ((float) ((Wn / 2 - Wn / 2 * saw) * rate)) * w * .5f;
                }
                shimD.push (x);
                shimOut = shimLp.tick (shimHp.tick (y)) * gShim.next();
                const float gv = gV.next() * (1 + dkV.next() * duckNow);
                L += wl * gv; R += wr * gv;
            }
            /* TAPE: a short delay swung slowly (wow) and quickly (flutter), a soft saturation, a little hiss */
            {
                const float wet = gTapeWet.next(), wow = gWow.next(), flt = gFlutter.next(), hiss = gHiss.next();
                wowPh += (float) (.55 / rate); if (wowPh >= 1) wowPh -= 1;
                flPh += (float) (6.7 / rate); if (flPh >= 1) flPh -= 1;
                const float d = (float) ((.008 + wow * std::sin (twoPi * wowPh) + flt * std::sin (twoPi * flPh)) * rate);
                tapeL.push (L); tapeR.push (R);
                if (wet > .0001f || hiss > 0)
                {
                    const float tk = 1 + S.tape.sat * 5, nrm = std::tanh (tk), post = 1 / (1 + S.tape.sat * .35f);
                    const float sl = std::tanh (tk * clampf (tapeL.read (d), -1, 1)) / nrm * post, sr = std::tanh (tk * clampf (tapeR.read (d), -1, 1)) / nrm * post;
                    const float hs = hiss > 0 ? hissHp.tick (rng.next() * 2 - 1) * hiss : 0.0f;
                    L = L * (1 - wet) + sl * wet + hs; R = R * (1 - wet) + sr * wet + hs;
                }
            }
            /* the PLATE (after TAPE, into the CHANNEL, as horizon.js wires it): cut below LOW CUT and above HIGH CUT, PRE-DELAY, its ring(s), MIX, DUCK */
            {
                const float gi = gPlIn.next(), gp = gPlPre.next(), go = gPlOut.next() * (1 + dkL.next() * duckNow);
                if (plateLive)
                {
                    float xl = plLp.tick (plHp.tick (pL * gi, 0), 0), xr = plLp.tick (plHp.tick (pR * gi, 1), 1);
                    plPreL.push (xl); plPreR.push (xr);
                    if (gp >= 1) { xl = plPreL.read (gp); xr = plPreR.read (gp); }
                    float yl = 0, yr = 0;
                    for (auto& s : plates)
                        if (s.live) { const float g = s.in.next(); yl += s.c[0].tick (xl * g) * s.scale; yr += s.c[1].tick (xr * g) * s.scale; }
                    L += yl * go; R += yr * go;
                }
            }
            /* the CHANNEL: LOW CUT, BODY, PRESENCE, AIR, a gentle compressor blended in alongside, WIDTH, MASTER; then the limiter */
            {
                L = chAir.tick (chPres.tick (chBody.tick (chLow.tick (L, 0), 0), 0), 0);
                R = chAir.tick (chPres.tick (chBody.tick (chLow.tick (R, 1), 1), 1), 1);
                float cl = L, cr = R; chComp.tick (cl, cr);
                const float dry = gChDry.next(), mk = gChMake.next();
                L = L * dry + cl * mk; R = R * dry + cr * mk;
                const float A = gWA.next(), B = gWB.next(), og = gOut.next();
                const float l2 = (A * L + B * R) * og, r2 = (A * R + B * L) * og;
                L = l2; R = r2;
                limiter.tick (L, R);
            }
            outL[done + i] = L; outR[done + i] = R;
            sq += .5 * (L * L + R * R);
        }
        done += n;
    }
    const float rms = total > 0 ? (float) std::sqrt (sq / total) : 0.0f;
    meterAcc += (rms - meterAcc) * (rms > meterAcc ? .5f : .08f);
    meter.store (meterAcc, std::memory_order_relaxed);
}

} // namespace horizon
