/* HORIZON's sound engine in plain C++ (no JUCE, nothing from an operating system), so the same file can be built into the
   DAW plugin (VST3 / AU) and, later, into WebAssembly for Mixtape 2 and the web.

   It plays the Salamander Grand (Yamaha C5, Alexander Holm, CC BY 3.0) as Mixtape 2's HORIZON does (mixtape2\Devices\horizon.js):
   four of its sixteen loudness layers (3, 7, 11, 15), every third note from A0 to C8; a key plays the nearest recording of its
   layer, re-pitched (cubic, as keyBuf does), louder within the layer as it is struck harder; the sustain pedal (CC 64) holds
   what is let go; a released key dies away over RELEASE. Then a hall (a Freeverb-style reverb, MIX) and LEVEL.

   Who calls what:
   - a loader (the plugin's own, or a web page's) decodes the recordings and hands them over with SampleSet::add, then gives the
     finished set to setSamples (any thread; the engine only reads it, and the set must outlive the engine's use of it);
   - the audio thread calls prepare once, then noteOn / noteOff / sustain / render; setParams any time (plain floats, read once
     a block). Nothing on the audio thread allocates or locks. */
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace horizon {

constexpr int kLayers[4] = { 3, 7, 11, 15 };          // the Salamander layers HORIZON uses
constexpr int kLayerTop[4] = { 45, 80, 105, 128 };    // the velocity each layer ends below (as LV_TOP)

/* the recordings, made ready once (mono, the silence before the hammer cut off, trimmed, faded) */
struct SampleSet
{
    struct Rec { std::vector<float> data; double rate = 44100.0; };
    Rec recs[128][4];                                   // [MIDI note][layer index 0..3]; empty where there is none

    /* one decoded recording: left and right (right may be null), its rate, its key and layer index (0..3). Prepares it as
       horizon.js loadSamples does: mono, starts 2 ms before the hammer (where it first reaches 1% of its peak), at most
       9 s (lows) … 3 s (highs), the last half second faded out */
    void add (int note, int layerIndex, const float* left, const float* right, int numSamples, double rate);
    bool has (int note, int layer) const { return note >= 0 && note < 128 && layer >= 0 && layer < 4 && ! recs[note][layer].data.empty(); }
    int count() const;
};

struct Params
{
    float levelDb = 0.0f;       // LEVEL: -48 … +6 dB
    float reverbMix = 0.3f;     // HALL: 0 (dry) … 1 (all hall)
    float releaseSec = 0.35f;   // RELEASE: 0.06 … 3 s (how long a let-go key takes to die away, to -60 dB)
};

/* a Freeverb-style hall: eight combs and four allpasses a side (Jezar's public-domain design), its buffers made in prepare */
class Hall
{
public:
    void prepare (double sampleRate);
    void process (float inL, float inR, float& outL, float& outR);
    void clear();
private:
    struct Comb { std::vector<float> buf; int pos = 0; float store = 0; };
    struct AllPass { std::vector<float> buf; int pos = 0; };
    Comb combL[8], combR[8];
    AllPass apL[4], apR[4];
    float feedback = 0.86f, damp = 0.3f;
};

class Engine
{
public:
    static constexpr int kMaxVoices = 64;

    void prepare (double sampleRate);
    void setSamples (const SampleSet* s) { samples.store (s, std::memory_order_release); }
    bool ready() const { return samples.load (std::memory_order_acquire) != nullptr; }
    void setParams (const Params& p) { params = p; }

    void noteOn (int note, int velocity);               // velocity 1 … 127
    void noteOff (int note);
    void sustain (bool down);
    void allNotesOff();                                 // quick fade of everything (PANIC, transport stop)

    /* adds nothing: writes numSamples into left and right */
    void render (float* left, float* right, int numSamples);

    /* how loud it plays (smoothed RMS of the output, 0 … ~1), for the sky; and a counter + velocity of the last key struck */
    float level() const { return meter.load (std::memory_order_relaxed); }
    int hits() const { return hitCount.load (std::memory_order_relaxed); }
    int lastHitVelocity() const { return hitVel.load (std::memory_order_relaxed); }
    bool pedalDown() const { return pedal.load (std::memory_order_relaxed); }

private:
    struct Voice
    {
        bool active = false, held = false, sustained = false, releasing = false;
        int note = 0;
        const float* data = nullptr; int length = 0;
        double pos = 0, step = 1;
        float amp = 0, gain = 0, gainStep = 0;          // gain: the release (1 → 0); gainStep: a fast fade when stolen
        float panL = 1, panR = 1;
        float attack = 0;                               // a 2 ms rise, so a re-struck key never clicks
        uint32_t age = 0;
    };
    Voice voices[kMaxVoices];
    std::atomic<const SampleSet*> samples { nullptr };
    Params params;
    Hall hall;
    double rate = 44100.0;
    std::atomic<bool> pedal { false };
    uint32_t clock = 0;
    float levelGain = 1.0f, meterAcc = 0.0f;
    std::atomic<float> meter { 0.0f };
    std::atomic<int> hitCount { 0 }, hitVel { 0 };

    Voice& freeVoice();
    void release (Voice& v);
};

} // namespace horizon
