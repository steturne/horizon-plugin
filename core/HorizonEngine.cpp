/* HORIZON's sound engine: see HorizonEngine.h for what it is and who calls what. */
#include "HorizonEngine.h"
#include <algorithm>

namespace horizon {

static inline float clampf (float v, float a, float b) { return std::min (b, std::max (a, v)); }

/* ---- the recordings ---- */
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
}

int SampleSet::count() const
{
    int c = 0;
    for (auto& k : recs) for (auto& l : k) if (! l.data.empty()) ++c;
    return c;
}

/* ---- the hall ---- */
void Hall::prepare (double sr)
{
    static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, apT[4] = { 556, 441, 341, 225 };
    const double k = sr / 44100.0;
    for (int i = 0; i < 8; ++i)
    {
        combL[i].buf.assign ((size_t) std::max (1, (int) (combT[i] * k)), 0.0f);
        combR[i].buf.assign ((size_t) std::max (1, (int) ((combT[i] + 23) * k)), 0.0f);   // the right a little longer: wide
    }
    for (int i = 0; i < 4; ++i)
    {
        apL[i].buf.assign ((size_t) std::max (1, (int) (apT[i] * k)), 0.0f);
        apR[i].buf.assign ((size_t) std::max (1, (int) ((apT[i] + 23) * k)), 0.0f);
    }
    clear();
}

void Hall::clear()
{
    for (auto* c : { combL, combR }) for (int i = 0; i < 8; ++i) { std::fill (c[i].buf.begin(), c[i].buf.end(), 0.0f); c[i].pos = 0; c[i].store = 0; }
    for (auto* a : { apL, apR }) for (int i = 0; i < 4; ++i) { std::fill (a[i].buf.begin(), a[i].buf.end(), 0.0f); a[i].pos = 0; }
}

void Hall::process (float inL, float inR, float& outL, float& outR)
{
    const float in = (inL + inR) * 0.015f;
    float l = 0, r = 0;
    auto comb = [this] (Comb& c, float x) {
        float y = c.buf[(size_t) c.pos];
        c.store = y * (1 - damp) + c.store * damp;
        c.buf[(size_t) c.pos] = x + c.store * feedback;
        if (++c.pos >= (int) c.buf.size()) c.pos = 0;
        return y;
    };
    auto allpass = [] (AllPass& a, float x) {
        float b = a.buf[(size_t) a.pos], y = b - x;
        a.buf[(size_t) a.pos] = x + b * 0.5f;
        if (++a.pos >= (int) a.buf.size()) a.pos = 0;
        return y;
    };
    for (int i = 0; i < 8; ++i) { l += comb (combL[i], in); r += comb (combR[i], in); }
    for (int i = 0; i < 4; ++i) { l = allpass (apL[i], l); r = allpass (apR[i], r); }
    outL = l * 3.0f; outR = r * 3.0f;
}

/* ---- the engine ---- */
void Engine::prepare (double sampleRate)
{
    rate = sampleRate > 0 ? sampleRate : 44100.0;
    hall.prepare (rate);
    for (auto& v : voices) v.active = false;
    pedal = false;
}

Engine::Voice& Engine::freeVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices) if (! v.active) return v;
    for (auto& v : voices) if (v.releasing && (best == nullptr || v.gain < best->gain)) best = &v;   // the quietest dying one
    if (best == nullptr) for (auto& v : voices) if (best == nullptr || v.age < best->age) best = &v;  // else the oldest
    return *best;
}

void Engine::release (Voice& v) { v.releasing = true; v.held = false; v.sustained = false; }

void Engine::noteOn (int n, int vel)
{
    const SampleSet* s = samples.load (std::memory_order_acquire);
    if (s == nullptr || n < 0 || n > 127 || vel <= 0) return;
    vel = std::min (127, vel);
    /* the layer, and how far into it (as layerOf) */
    int li = 0; while (li < 3 && vel >= kLayerTop[li]) ++li;
    const int lo = li ? kLayerTop[li - 1] : 1;
    const float frac = clampf ((float) (vel - lo) / (float) (kLayerTop[li] - lo), 0, 1), amp = (0.62f + 0.38f * frac) * 1.15f;
    /* the nearest recording of that layer, falling back to the next note, then to another layer (as nearest) */
    static const int D[7] = { 0, 1, -1, 2, -2, 3, -3 };
    const SampleSet::Rec* rec = nullptr; int src = 0;
    const int base = std::min (108, std::max (21, (int) std::floor ((n - 21) / 3.0 + 0.5) * 3 + 21));
    for (int d : D)
    {
        const int order[5] = { li, 0, 1, 2, 3 };
        for (int l : order)
        {
            const int k = base + d * 3;
            if (s->has (k, l)) { rec = &s->recs[k][l]; src = k; break; }
        }
        if (rec) break;
    }
    if (rec == nullptr) return;
    /* the same key struck again: the old one fades out quickly (60 ms), as a damper would */
    for (auto& v : voices)
        if (v.active && v.note == n && v.gainStep == 0) v.gainStep = (float) (v.gain / (0.06 * rate)), v.releasing = true;
    Voice& v = freeVoice();
    v = Voice{};
    v.active = true; v.held = true; v.note = n; v.data = rec->data.data(); v.length = (int) rec->data.size() - 3;
    v.step = std::pow (2.0, (n - src) / 12.0) * rec->rate / rate;
    v.amp = amp; v.gain = 1; v.age = ++clock;
    const float pan = clampf ((n - 64) / 44.0f, -1, 1) * 0.45f;                    // low keys a little left, high right (panOf)
    v.panL = std::sqrt (0.5f * (1 - pan)) * 1.41421356f; v.panR = std::sqrt (0.5f * (1 + pan)) * 1.41421356f;
    hitVel.store (vel, std::memory_order_relaxed); hitCount.fetch_add (1, std::memory_order_relaxed);
}

void Engine::noteOff (int n)
{
    for (auto& v : voices)
        if (v.active && v.held && v.note == n)
        {
            v.held = false;
            if (pedal) v.sustained = true; else release (v);
        }
}

void Engine::sustain (bool down)
{
    pedal = down;
    if (! down) for (auto& v : voices) if (v.active && v.sustained && ! v.held) release (v);
}

void Engine::allNotesOff()
{
    for (auto& v : voices) if (v.active) { v.releasing = true; v.gainStep = (float) (v.gain / (0.02 * rate)); }
    pedal = false;
}

void Engine::render (float* outL, float* outR, int n)
{
    const Params p = params;
    const float relCoef = (float) std::exp (-6.9078 / (clampf (p.releaseSec, 0.02f, 10.0f) * rate));   // -60 dB in RELEASE
    const float targetGain = p.levelDb <= -47.9f ? 0.0f : std::pow (10.0f, p.levelDb / 20.0f) * 0.5f;
    const float mix = clampf (p.reverbMix, 0, 1), dryG = 1 - 0.55f * mix, wetG = 1.25f * mix;
    const float attStep = (float) (1.0 / (0.002 * rate));

    for (int i = 0; i < n; ++i) { outL[i] = 0; outR[i] = 0; }
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float* x = v.data;
        for (int i = 0; i < n; ++i)
        {
            const int j = (int) v.pos;
            if (j >= v.length) { v.active = false; break; }
            const float f = (float) (v.pos - j), y0 = x[j > 0 ? j - 1 : 0], y1 = x[j], y2 = x[j + 1], y3 = x[j + 2];
            const float y = y1 + 0.5f * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
            if (v.attack < 1) v.attack = std::min (1.0f, v.attack + attStep);
            const float a = y * v.amp * v.gain * v.attack;
            outL[i] += a * v.panL; outR[i] += a * v.panR;
            v.pos += v.step;
            if (v.gainStep > 0) { v.gain -= v.gainStep; if (v.gain <= 0) { v.active = false; break; } }
            else if (v.releasing) { v.gain *= relCoef; if (v.gain < 0.0005f) { v.active = false; break; } }
        }
    }
    /* the hall, LEVEL (smoothed over a few ms), and a soft ceiling so many loud notes never clip hard */
    double sq = 0;
    const float smooth = (float) (1.0 - std::exp (-1.0 / (0.01 * rate)));
    for (int i = 0; i < n; ++i)
    {
        float wl, wr; hall.process (outL[i], outR[i], wl, wr);
        levelGain += (targetGain - levelGain) * smooth;
        float l = (outL[i] * dryG + wl * wetG) * levelGain, r = (outR[i] * dryG + wr * wetG) * levelGain;
        auto soft = [] (float s) { const float a = std::abs (s); return a < 0.8f ? s : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), s); };
        l = soft (l); r = soft (r);
        outL[i] = l; outR[i] = r; sq += 0.5 * (l * l + r * r);
    }
    const float rms = n > 0 ? (float) std::sqrt (sq / n) : 0.0f;
    meterAcc += (rms - meterAcc) * (rms > meterAcc ? 0.5f : 0.08f);
    meter.store (meterAcc, std::memory_order_relaxed);
}

} // namespace horizon
