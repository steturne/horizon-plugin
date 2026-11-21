/* Small sound parts for HORIZON's engine, plain C++ (no JUCE). They behave as the browser's Web Audio nodes HORIZON was first made
   with (mixtape2\Devices\horizon.js), so the plugin sounds as Mixtape's HORIZON does:
   - Biquad: the BiquadFilterNode's own formulas (lowpass / highpass take Q in dB, as the browser does; peaking, shelves, allpass)
   - Compressor: a DynamicsCompressorNode in spirit (threshold, knee, ratio, attack, release, and its automatic make-up gain)
   - Smooth: a value gliding to where it is set, as setTargetAtTime does
   - Delay: a delay line read at any (fractional) time */
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace horizon {

constexpr double kPi = 3.14159265358979323846;
inline float clampf (float v, float a, float b) { return std::min (b, std::max (a, v)); }
inline float dbToGain (float d) { return std::pow (10.0f, d / 20.0f); }

/* a value that glides toward its target with a time constant (seconds), sample by sample */
struct Smooth
{
    float value = 0, target = 0, coef = 0;
    void setup (double rate, double tau) { coef = tau <= 0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (tau * rate))); }
    void reset (float v) { value = target = v; }
    inline float next() { value += (target - value) * coef; return value; }
    /* glide over a whole block at once (for things set once a block) */
    inline float block (int n) { value += (target - value) * (1.0f - std::pow (1.0f - coef, (float) n)); return value; }
};

struct Biquad
{
    enum Type { lowpass, highpass, peaking, lowshelf, highshelf, allpass };
    Type type = lowpass;
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };
    float lastF = -1, lastQ = -1000, lastG = -1000;

    /* the browser's formulas (Web Audio spec, "BiquadFilterNode": Q in dB for lowpass / highpass, shelves with slope 1) */
    void set (Type t, double rate, float freq, float q, float gainDb = 0)
    {
        if (t == type && freq == lastF && q == lastQ && gainDb == lastG) return;
        type = t; lastF = freq; lastQ = q; lastG = gainDb;
        const double f = std::min ((double) freq, rate * 0.4999), w0 = 2 * kPi * f / rate, cw = std::cos (w0), sw = std::sin (w0);
        const double A = std::pow (10.0, gainDb / 40.0);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (t)
        {
            case lowpass: case highpass:
            {
                const double alpha = sw / (2 * std::pow (10.0, q / 20.0));
                if (t == lowpass) { B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; }
                else { B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; }
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            }
            case peaking:
            {
                const double alpha = sw / (2 * std::max (1e-4, (double) q));
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            }
            case allpass:
            {
                const double alpha = sw / (2 * std::max (1e-4, (double) q));
                B0 = 1 - alpha; B1 = -2 * cw; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            }
            case lowshelf: case highshelf:
            {
                const double alpha = sw / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * alpha;
                if (t == lowshelf)
                {
                    B0 = A * ((A + 1) - (A - 1) * cw + sa); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sa);
                    A0 = (A + 1) + (A - 1) * cw + sa; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sa;
                }
                else
                {
                    B0 = A * ((A + 1) + (A - 1) * cw + sa); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sa);
                    A0 = (A + 1) - (A - 1) * cw + sa; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sa;
                }
                break;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    inline float tick (float x, int ch = 0)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

/* a delay line (one channel), read anywhere up to its length, between samples by straight interpolation */
struct Delay
{
    std::vector<float> buf; int pos = 0;
    void setup (int maxSamples) { buf.assign ((size_t) std::max (4, maxSamples + 4), 0.0f); pos = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) pos] = x; if (++pos >= (int) buf.size()) pos = 0; }
    /* d samples ago (d >= 1 once pushed this sample: read before push for d >= 0) */
    inline float read (float d) const
    {
        const int n = (int) buf.size();
        d = clampf (d, 0.0f, (float) (n - 3));
        float p = (float) pos - d; while (p < 0) p += (float) n;
        const int i = (int) p; const float f = p - (float) i;
        return buf[(size_t) i] * (1 - f) + buf[(size_t) ((i + 1) % n)] * f;
    }
};

/* a compressor as the browser's DynamicsCompressorNode works: the louder of the two sides is measured, a soft knee, the gain
   reduction follows with attack and release, and (as the browser does) the result is lifted by a make-up gain worked out from
   the curve, so the compressed sound is about as loud as before */
struct Compressor
{
    float threshold = -24, knee = 30, ratio = 12, attack = 0.003f, release = 0.25f;
    double rate = 44100; float env = 0; /* gain reduction now, dB (<= 0) */
    void setup (double r) { rate = r; env = 0; }
    float curve (float inDb) const
    {
        const float lo = threshold - knee / 2, hi = threshold + knee / 2;
        if (inDb <= lo) return inDb;
        if (inDb >= hi || knee <= 0) return threshold + (inDb - threshold) / ratio;
        const float x = inDb - lo;
        return inDb + (1 / ratio - 1) * x * x / (2 * knee);
    }
    float makeup() const { const float full = curve (0.0f); return std::pow (dbToGain (-full), 0.6f); }
    /* one sample at a time: coefficients worked out by ready() whenever the settings change */
    float aC = 0, rC = 0, mk = 1;
    void ready() { aC = (float) std::exp (-1.0 / (std::max (0.0005f, attack) * rate)); rC = (float) std::exp (-1.0 / (std::max (0.005f, release) * rate)); mk = makeup(); }
    inline void tick (float& l, float& r)
    {
        const float pk = std::max (std::abs (l), std::abs (r)), inDb = pk > 1e-6f ? 20 * std::log10 (pk) : -120.0f;
        const float want = curve (inDb) - inDb;
        env = want < env ? aC * env + (1 - aC) * want : rC * env + (1 - rC) * want;
        const float g = dbToGain (env) * mk;
        l *= g; r *= g;
    }
    /* stereo in place */
    void process (float* L, float* R, int n)
    {
        const float aC = (float) std::exp (-1.0 / (std::max (0.0005f, attack) * rate)), rC = (float) std::exp (-1.0 / (std::max (0.005f, release) * rate)), mk = makeup();
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (L[i]), std::abs (R[i])), inDb = pk > 1e-6f ? 20 * std::log10 (pk) : -120.0f;
            const float want = curve (inDb) - inDb;
            env = want < env ? aC * env + (1 - aC) * want : rC * env + (1 - rC) * want;
            const float g = dbToGain (env) * mk;
            L[i] *= g; R[i] *= g;
        }
    }
};

/* the browser's StereoPannerNode for a one-channel sound: pan -1 … 1 → left and right gains (equal power) */
inline void panGains (float pan, float& l, float& r)
{
    const float x = (clampf (pan, -1, 1) + 1) * 0.5f;
    l = std::cos (x * (float) kPi / 2); r = std::sin (x * (float) kPi / 2);
}

/* a small fast random number source (the same everywhere, nothing from the system) */
struct Rng
{
    unsigned s = 0x9E3779B9u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 16777216.0f; }   // 0 … 1
};

} // namespace horizon
