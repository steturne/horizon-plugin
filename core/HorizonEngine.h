/* HORIZON's sound engine in plain C++ (no JUCE, nothing from an operating system), so the same files can be built into the
   DAW plugin (VST3 / AU) and, later, into WebAssembly for Mixtape 2 and the web. It is Mixtape 2's HORIZON
   (mixtape2\Devices\horizon.js) note for note: the same recordings, the same shaping, the same effects in the same order, with the
   same formulas (HorizonSettings.h), so a sound made in one sounds the same in the other.

   The piano: the Salamander Grand (Yamaha C5, Alexander Holm, CC BY 3.0), four of its sixteen loudness layers (3, 7, 11, 15), every
   third note A0 … C8. A key plays the nearest recording, re-pitched; each note is split into its STRIKE (the hammer: its first
   LENGTH) and its TAIL (the ring), shaped apart (the SHAPER: PUNCH, BRIGHT, HAMMER = the strike from a harder recording, SOFT TAIL =
   the tail from a softer one, WARMTH, DARKEN, SUSTAIN / ENDLESS, SPARKLE = glints an octave or two up).
   Then, as horizon.js wires it: each half through its TUBE and into the DELAY and the REVERB by its own sends; the DELAY (ping-pong,
   in the DAW's tempo), the REVERB (Crazy Dream Lite's hall: an 8-line feedback delay network, with SHIMMER an octave up and FREEZE),
   DUCK (the echoes and the hall step back while the piano plays), TAPE, the CHANNEL (low cut, body, presence, air, a parallel glue
   compressor, width, MASTER) and a limiter.

   Who calls what:
   - a loader decodes the recordings, hands each to SampleSet::add, calls SampleSet::finish, then gives the set to setSamples
     (any thread; the engine only reads it; it must outlive the engine's use of it);
   - the audio thread calls prepare once, then setSettings / setTempo, noteOn / noteOff / pedal, render. Nothing on the audio
     thread allocates or locks. */
#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include "Dsp.h"
#include "HorizonSettings.h"

namespace horizon {

constexpr int kLayers[4] = { 3, 7, 11, 15 };          // the Salamander layers HORIZON uses
constexpr int kLayerTop[4] = { 45, 80, 105, 128 };    // the velocity each layer ends below (as LV_TOP)

/* the recordings, made ready once (mono, the silence before the hammer cut off, trimmed, faded), with what is measured on them */
struct SampleSet
{
    static constexpr double kEnvStep = 0.005;           // the loudness is kept every 5 ms
    struct Rec
    {
        std::vector<float> data; double rate = 44100.0;
        std::vector<float> env;                         // mean square of each 5 ms
        float strike = 0, tail = 0;                     // loudness of the strike (first 40 ms) and of the tail (0.25 … 0.75 s), as horizon.js meas
        double seconds() const { return (double) (data.size() > 3 ? data.size() - 3 : 0) / rate; }
        /* the loudness (RMS) around t seconds, over w seconds (as horizon.js envAt) */
        float envAt (double t, double w) const;
    };
    Rec recs[128][4];                                   // [MIDI note][layer index 0..3]; empty where there is none
    int8_t lag[128][4][4] = {};                         // two layers of one note lined up: samples (±3 ms) the second is late by

    void add (int note, int layerIndex, const float* left, const float* right, int numSamples, double rate);
    void finish();                                      // after the last add: measures how the layers line up (as lagOf)
    bool has (int note, int layer) const { return note >= 0 && note < 128 && layer >= 0 && layer < 4 && ! recs[note][layer].data.empty(); }
    int count() const;
    /* the nearest recording of a layer, falling back to the next note, then another layer (as horizon.js nearest); -1 if none */
    int nearest (int note, int layer, int& layerFound) const;
};

/* Crazy Dream Lite's hall (crazy-dream-lite.js), as HORIZON uses it: all wet, an 8-line feedback delay network with a Householder
   mix, diffusion, damping and drifting delays; CHIME (resonance) when SHIMMER is up; FREEZE holds it */
class Hall
{
public:
    void prepare (double rate);
    void clear();
    /* size 0..1 (Crazy Dream's), decay seconds, predelay seconds, low cut Hz, resonance 0..1, frozen */
    void set (float size, float decay, float predelay, float lowCut, float resonance, bool frozen);
    void process (float inL, float inR, float& outL, float& outR);
private:
    static constexpr int N = 8;
    double rate = 44100;
    Delay pre, line[N];
    Biquad lowCut, highCut, diff[5], damp[N], peak[N];
    Smooth lineT[N], gain, sumG, preT, inGate;
    float depth = 0, lfoHz = 0.37f, ph[2] = { 0, 0.3f }, resDb = 0;
    bool frozen = false;
    float last[N] = {};
};

class Engine
{
public:
    static constexpr int kMaxVoices = 80, kMaxGrains = 25;

    void prepare (double sampleRate);
    void setSamples (const SampleSet* s) { samples.store (s, std::memory_order_release); }
    bool ready() const { return samples.load (std::memory_order_acquire) != nullptr; }
    void setSettings (const Settings& s) { S = s; }
    void setTempo (double bpm) { if (bpm > 20 && bpm < 400) tempo = bpm; }

    /* the keys and the pedal, as horizon.js key(): a key let go while the pedal is down rings until the pedal comes up */
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void pedal (bool down);
    void allNotesOff();                                 // quick fade of every note (PANIC, the DAW stopping)

    /* writes numSamples into left and right */
    void render (float* left, float* right, int numSamples);

    float level() const { return meter.load (std::memory_order_relaxed); }
    int hits() const { return hitCount.load (std::memory_order_relaxed); }
    int lastHitVelocity() const { return hitVel.load (std::memory_order_relaxed); }
    bool pedalDown() const { return pedalIsDown.load (std::memory_order_relaxed); }

private:
    struct Src { const SampleSet::Rec* rec = nullptr; double pos = 0, step = 1, pr = 1; double start = 0; };   // pos in the recording's samples; start: seconds after the key
    struct Grain { const SampleSet::Rec* rec = nullptr; double pos = 0, step = 1; float when = 0, len = 0, lv = 0, l = 0, r = 0; };
    struct Voice
    {
        bool active = false, released = false, two = false, endless = false, darken = false;
        int note = 0; uint32_t age = 0;
        double t = 0;                                   // seconds since the key
        Src sS, sT;                                     // the strike's recording and the tail's (the same one when not two)
        float aS = 0, aT = 0, aH = 0, att = .002f, L = .03f, X = .02f, C = 0, tEnd = 0, tTail = 0;
        float panL = .707f, panR = .707f;
        float susPow = 0, susRef = 1, lift = 1;         // SUSTAIN: the tail lifted back toward where it began
        /* ENDLESS: a stretch of the tail (A … A+len key-seconds) levelled and looped with a crossfade XF; its level reference */
        float loopA = .6f, loopN = 1, loopXF = .25f, loopAt = .85f, t1 = 0, loopRef = 1, gLoopA = 1, gLoopB = 1;
        Biquad dk; float dkTarget = 18000;
        float relG = 1, relCoef = 1, stopAt = 1e9f;     // the release (or a quick kill)
        Grain grains[kMaxGrains]; int nGrains = 0;
    };
    Voice voices[kMaxVoices];
    std::atomic<const SampleSet*> samples { nullptr };
    Settings S;
    double rate = 44100.0, tempo = 120.0;
    uint32_t clock = 0;
    Rng rng;
    bool down[128] = {}, sus[128] = {}, pedalOn = false;
    std::atomic<bool> pedalIsDown { false };
    std::atomic<float> meter { 0.0f };
    std::atomic<int> hitCount { 0 }, hitVel { 0 };
    float meterAcc = 0;

    /* the buses the voices play into, and the chain after them (made in prepare) */
    std::vector<float> busS[2], busT[2], busG[2];
    Biquad sDamp, sBright, tWarm, tDamp, sparkHi;
    struct Half { Biquad low, high; Smooth through, pre, post, del, rev; };
    Half HS, HT;
    Smooth gPiano, gSpark, gStrike, gGate, gDelIn, gVerbIn, gD, gV, gShim, gTapeWet, gWow, gFlutter, gHiss, gChDry, gChMake, gWA, gWB, gOut,
           dkV, dkD, dDelayT;
    float tubeBias = .19f, tubeDrive = 5.2f;
    /* DUCK: how loud the piano plays, as a slow envelope */
    Biquad duckEnv; float duckNow = 0;
    /* DELAY: ping-pong */
    Delay dlL, dlR; Biquad dTone, fl, fr; float fbL = 0, fbR = 0;
    /* REVERB and its shimmer (an octave up: two delay lines swept by a sawtooth half a sweep apart, each faded round its jump) */
    Hall hall; Delay shimD; Biquad shimHp, shimLp; float shimPh = 0, shimOut = 0;
    /* TAPE */
    Delay tapeL, tapeR; Biquad hissHp; float wowPh = 0, flPh = 0;
    /* the CHANNEL and the limiter */
    Biquad chLow, chBody, chPres, chAir; Compressor chComp, limiter;

    Voice& freeVoice();
    void start (Voice& v, int note, int vel);
    void release (Voice& v, float seconds);
    void kill (Voice& v, float k);
    void renderVoice (Voice& v, int n);
    void update (int n);                               // the settings into the chain, once a block
};

} // namespace horizon
