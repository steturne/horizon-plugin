/* A convolver (the browser's ConvolverNode, in plain C++): plays a long "ring" (an impulse response) under a sound, for HORIZON's
   PLATE (a plate's dense ring, made from noise: plateIR in horizon.js) and the hall's BLOOM (a rising swell: renderSwell in
   crazy-dream-lite.js). A ring of seconds is tens of thousands of taps, so it is done the fast way: the ring cut into pieces of B
   samples, each turned into its spectrum once (an FFT), and the sound's spectrum multiplied by all of them each B samples
   (uniformly partitioned overlap-save). The sound comes out B samples late (the PLATE takes that off its pre-delay).
   The ring can be made a piece at a time (addPiece), so making a new one never holds the audio up; nothing here allocates
   after setup. */
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace horizon {

/* a complex FFT of a power-of-two size (radix 2, tables made once) */
class FFT
{
public:
    void setup (int size)
    {
        n = size; int bits = 0; while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        cs.assign ((size_t) n / 2, 0); sn.assign ((size_t) n / 2, 0);
        for (int i = 0; i < n / 2; ++i) { cs[(size_t) i] = (float) std::cos (2 * 3.14159265358979323846 * i / n); sn[(size_t) i] = (float) -std::sin (2 * 3.14159265358979323846 * i / n); }
    }
    /* in place; inverse = true for the way back (not scaled: divide by n) */
    void run (float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i) { const int j = rev[(size_t) i]; if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cs[(size_t) (k * step)], wi = inverse ? -sn[(size_t) (k * step)] : sn[(size_t) (k * step)];
                    const int p = i + k, q = p + half;
                    const float tr = re[q] * wr - im[q] * wi, ti = re[q] * wi + im[q] * wr;
                    re[q] = re[p] - tr; im[q] = im[p] - ti; re[p] += tr; im[p] += ti;
                }
        }
    }
    int size() const { return n; }
private:
    int n = 0; std::vector<int> rev; std::vector<float> cs, sn;
};

/* one channel: the sound in, the ring's pieces, the sound out B samples later */
class PartConv
{
public:
    /* B: piece size (power of two); maxTaps: the longest ring it will ever hold */
    void setup (int pieceSize, int maxTaps)
    {
        B = pieceSize; N = 2 * B; K = B + 1;
        P = (maxTaps + B - 1) / B;
        fft.setup (N);
        Hre.assign ((size_t) P * K, 0); Him.assign ((size_t) P * K, 0);
        Fre.assign ((size_t) P * K, 0); Fim.assign ((size_t) P * K, 0);
        frame.assign ((size_t) N, 0); out.assign ((size_t) B, 0);
        wr.assign ((size_t) N, 0); wi.assign ((size_t) N, 0); ar.assign ((size_t) K, 0); ai.assign ((size_t) K, 0);
        pieces = 0; clear();
    }
    void clear() { std::fill (Fre.begin(), Fre.end(), 0.0f); std::fill (Fim.begin(), Fim.end(), 0.0f); std::fill (frame.begin(), frame.end(), 0.0f); std::fill (out.begin(), out.end(), 0.0f); pos = 0; head = 0; }
    /* a new ring: forget the old pieces (the sound's own history is kept) */
    void beginRing() { pieces = 0; }
    bool full() const { return pieces >= P; }
    int pieceSize() const { return B; }
    /* the ring's next B samples */
    void addPiece (const float* h)
    {
        if (pieces >= P) return;
        for (int i = 0; i < N; ++i) { wr[(size_t) i] = i < B ? h[i] : 0.0f; wi[(size_t) i] = 0; }
        fft.run (wr.data(), wi.data(), false);
        float* hr = &Hre[(size_t) pieces * K]; float* hi = &Him[(size_t) pieces * K];
        for (int k = 0; k < K; ++k) { hr[k] = wr[(size_t) k] / (float) N; hi[k] = wi[(size_t) k] / (float) N; }   // (the 1/N of the way back, once)
        ++pieces;
    }
    /* one sample in, one out (B samples late) */
    inline float tick (float x)
    {
        frame[(size_t) (B + pos)] = x;
        const float y = out[(size_t) pos];
        if (++pos >= B) { block(); pos = 0; }
        return y;
    }
private:
    int B = 0, N = 0, K = 0, P = 0, pieces = 0, pos = 0, head = 0;
    FFT fft;
    std::vector<float> Hre, Him, Fre, Fim, frame, out, wr, wi, ar, ai;
    void block()
    {
        /* the last 2B samples' spectrum, kept in the line of past spectra */
        for (int i = 0; i < N; ++i) { wr[(size_t) i] = frame[(size_t) i]; wi[(size_t) i] = 0; }
        for (int i = 0; i < B; ++i) frame[(size_t) i] = frame[(size_t) (B + i)];
        fft.run (wr.data(), wi.data(), false);
        head = (head + P - 1) % P;
        float* fr = &Fre[(size_t) head * K]; float* fi = &Fim[(size_t) head * K];
        for (int k = 0; k < K; ++k) { fr[k] = wr[(size_t) k]; fi[k] = wi[(size_t) k]; }
        /* each piece of the ring times the sound's spectrum from that many blocks ago */
        std::fill (ar.begin(), ar.end(), 0.0f); std::fill (ai.begin(), ai.end(), 0.0f);
        for (int p = 0; p < pieces; ++p)
        {
            const int s = (head + p) % P;
            const float *hr = &Hre[(size_t) p * K], *hi = &Him[(size_t) p * K], *xr = &Fre[(size_t) s * K], *xi = &Fim[(size_t) s * K];
            for (int k = 0; k < K; ++k) { ar[(size_t) k] += xr[k] * hr[k] - xi[k] * hi[k]; ai[(size_t) k] += xr[k] * hi[k] + xi[k] * hr[k]; }
        }
        /* back (a real sound: the upper half mirrors the lower), keeping the last B samples (overlap-save) */
        for (int k = 0; k < K; ++k) { wr[(size_t) k] = ar[(size_t) k]; wi[(size_t) k] = ai[(size_t) k]; }
        for (int k = K; k < N; ++k) { wr[(size_t) k] = ar[(size_t) (N - k)]; wi[(size_t) k] = -ai[(size_t) (N - k)]; }
        fft.run (wr.data(), wi.data(), true);
        for (int i = 0; i < B; ++i) out[(size_t) i] = wr[(size_t) (B + i)];
    }
};

} // namespace horizon
