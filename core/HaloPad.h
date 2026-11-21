/* HORIZON's PAD and DRONE: Mixtape 2's HALO (mixtape2\Devices\halo.js) in plain C++ (no JUCE), as HORIZON plays it.
   Each character (WARM GLASS CHOIR STRINGS AIR ORGAN BOWL SHRUTI) is a few seconds of a huge, smeared, slightly out-of-tune
   ensemble that loops without a seam, made by the PADsynth method (each harmonic spread into a band, random phases, one inverse
   FFT), one table for every note at its own pitch, so a note plays it at speed 1. A note: each lit character's table (and each
   VOICING note's), a soft sine SUB, AIR and SPARKLE layers, through its own filter (TONE, opening with the SWELL) and its own swell.
   Then for all notes together: the motion (BREATHE, SHIMMER, DRIFT, PULSE, WANDER), TEXTURE's breath and saturation, WIDTH, its
   own hall (SPACE: an 8-line feedback delay network), the drift's pan, the level.

   The tables take milliseconds each to make, so they are never made on the audio thread: PadTables keeps a pool of them, the audio
   thread asks for one (a note sounds from the nearest octave's table until its own is ready, then crossfades over), and a worker
   thread (the plugin's) calls PadTables::work to make them. */
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "Dsp.h"
#include "HorizonSettings.h"

namespace horizon {

class PadTables
{
public:
    static constexpr int TN = 1 << 17, kSlots = 96, kChars = 8;
    PadTables();
    /* the engine's rate: tables are made at it (a new rate forgets the old ones) */
    void setRate (double r);
    /* ---- the audio thread ---- */
    /* the slot holding this character's table for this note, held (call drop when done), or -1 (then it is asked for) */
    int take (int ch, int note);
    void drop (int slot);
    const float* data (int slot) const { return pool.get() ? pool.get() + (size_t) slot * TN : nullptr; }
    /* a character's octave tables (C2 … C6): asked for once the PAD is used, kept */
    void wantBases (int ch);
    /* ---- the worker thread: makes one asked-for table; true if it made one (call again soon), false if there was nothing ---- */
    bool work();

private:
    double rate = 48000;
    std::atomic<int> gen { 0 };
    std::unique_ptr<float[]> pool;                     // kSlots tables, made the first time one is needed (on the worker)
    std::atomic<bool> poolReady { false };
    std::atomic<int> where[kChars][128];               // the slot holding (character, note), or -1
    std::atomic<int> refs[kSlots];
    std::atomic<uint32_t> used[kSlots];
    std::atomic<bool> pinned[kSlots];
    std::atomic<bool> asked[kChars][128];
    int slotKey[kSlots];                               // (character × 128 + note) a slot holds, or -1 (the worker's own)
    std::atomic<uint32_t> stamp { 1 };
    /* the asked-for tables, one producer (audio thread) and one consumer (worker) */
    static constexpr int kQ = 512;
    std::atomic<int> q[kQ]; std::atomic<int> qHead { 0 }, qTail { 0 };
    void ask (int ch, int note, bool pin);
    void make (int ch, int note, float* out) const;
};

class HaloPad
{
public:
    struct Params
    {
        bool ch[8] = { true, false, false, false, false, false, false, false };
        float lv[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
        int motion = 1, voicing = 0;
        float tone = .45f, width = .65f, swell = .55f, amt = .35f, space = .55f, tex = .25f, speed = .75f, sub = .3f, air = 0, spark = 0, vol = .5f;
    };
    void prepare (double rate, PadTables* tables);
    void setParams (const Params& p, double bpm);
    /* a note: velocity 1 … 127; drone notes bypass the voicing and go through the drone's own level */
    void noteOn (int note, int vel, bool drone);
    void noteOff (int note, bool drone);
    void killAll();
    void setDroneLevel (float g) { droneG.target = g; }
    /* adds into left and right */
    void render (float* L, float* R, int n);
    bool busy() const;

private:
    static constexpr int kVoices = 24, kLayers = 4, kNotes = 4;
    struct Part { int slot = -1, base = -1, baseNote = 0; double pos[2] = { 0, 0 }, bpos[2] = { 0, 0 }, step = 1; float xf = 0; };   // pos: its own table; bpos: the octave's; xf: 0 = the octave's, 1 = its own
    struct Layer { int ch = -1; float g = 0, target = 0, step = 0; bool dying = false; Part part[kNotes]; };
    struct Voice
    {
        bool active = false, released = false, drone = false, killed = false;
        int n = 0, nNotes = 0, notes[kNotes] = {}; float noteLv[kNotes] = {}; uint32_t age = 0;
        Layer lay[kLayers]; Part air, spark; int airCh = 4, sparkCh = 0; float airG = 0, sparkG = 0;
        double subPh = 0, subStep = 0; float subG = 0;
        Biquad f; float cut = 1000, cutFrom = 300, t = 0, fT = .1f;
        float vca = 0, peak = 0, vcaCoef = 0;
        double endAt = 1e9;
    };
    Voice voices[kVoices];
    PadTables* T = nullptr;
    Params P; double rate = 48000, bpm = 120; uint32_t clock = 0; Rng rng;
    bool chWas[8] = { true, false, false, false, false, false, false, false };
    /* all notes together */
    Smooth busG, hissG, droneG, bfDet, gateBase, gateLfo, preG, postG, wA, wB, dryG, wetG, panL, panW, volG, shimG, driftD;
    Biquad bf, hlp, hissHp; Delay vibL, vibR; float drive = 1.25f;
    double ph[5] = { 0, 0, 0, 0, 0 };                 // breathe, drift, pan, shimmer, pulse
    double wanderAt = 0, nowT = 0; float wanderDet = 0, wanderPan = 0;
    /* its hall */
    Delay line[8]; Biquad damp[8]; float lineGain[8] = {}; float dampF = 5000;
    std::vector<float> bus[4];                         // the notes (L, R) and the drone's (L, R)

    void startPart (Part& p, int ch, int note);
    inline float readPart (const Part& p, int side) const;
    void freePart (Part& p);
    void addLayer (Voice& v, int ch, float fade);
    void killVoice (Voice& v, float tau);
    void releaseVoice (Voice& v);
    float layerLevel (int ch) const;
};

} // namespace horizon
