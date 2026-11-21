/* Every HORIZON setting, as Mixtape 2's HORIZON keeps them (mixtape2\Devices\horizon.js, PV and its SHAPE0 / TAPE0 / CHAN0 / PLATE0):
   each knob 0 … 1, switches true / false, modes by number. The plugin fills one of these from its parameters each block; the
   engine turns them into sound with the same formulas as horizon.js (below), so a knob means the same in both. */
#pragma once
#include <cmath>

namespace horizon {

struct Settings
{
    int piano = 0;                    // 0 ON, 1 FX ONLY (the piano itself silent, its effects heard), 2 OFF
    float level = .75f, damp = .6f, release = .25f;
    struct Shape { float punch = .6f, bright = .5f, len = .53f, hammer = 0, tail = .6f, soft = 0, warmth = 0, darken = 0, sustain = 0, sparkle = 0,
                   tubeS = 1, delS = 1, revS = 1, tubeT = 1, delT = 1, revT = 1; } shape;
    struct Tube { bool on = false; float drive = .3f, warmth = .5f, level = .5f; } tube;
    struct Rev { bool on = false; int kind = 1; float time = .35f, swell = .45f, rep = .45f, mix = .5f; bool sync = false, rings = true; } rev;   // kind: 0 BACK, 1 P+B, 2 ECHO
    struct Del { bool on = false; float time = .45f, rep = .3f, tone = .5f, mix = .25f; bool sync = true; int div = 1; float duck = 0; } del;   // div: 0 1/4, 1 1/8·, 2 1/8
    struct Verb { bool on = true; float mix = .4f, size = .6f, shim = 0, bloom = 0; bool freeze = false; float pre = .12f, low = .35f, duck = 0; } verb;
    struct Tape { bool on = false; float wobble = .3f, sat = .3f, hiss = .1f; } tape;
    struct Chan { float low = 0, body = .5f, pres = .5f, air = .5f, glue = .25f, blend = .5f, width = .667f; bool mono = false; float out = .833f; } chan;
    struct Plate { bool on = false; float mix = .35f, decay = .5f, pre = .15f, low = .66f, high = .67f, duck = 0; } plate;
    /* the PAD (HALO's sound) and the DRONE: kept here now, sounding from step 4 */
    struct Pad { bool on = false; float level = .5f, rev = .25f, low = 0, duck = 0, dlevel = .6f;
                 bool ch[8] = { true, false, false, false, false, false, false, false };   // WARM GLASS CHOIR STRINGS AIR ORGAN BOWL SHRUTI
                 float lv[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };                                       // each character's own level
                 int motion = 1, voicing = 0, range = 0, oct = 1, key = 0;                // motion: STILL BREATHE SHIMMER DRIFT PULSE WANDER; voicing: SINGLE OCTAVE FIFTH WIDE SUS; range: ALL / LEFT; oct: -1 0 +1
                 bool root = false, fifth = false, octave = false, sub = false;
                 float swell = .55f, tone = .45f, width = .65f, amt = .35f, speed = .75f, space = .55f, tex = .25f, subLv = .3f, air = 0, spark = 0; } pad;
};

/* ---- the knobs' values (0 … 1) to sound: horizon.js's own formulas ---- */
inline float cutOf (float d)   { return 500.0f * std::pow (40.0f, d); }              // DAMP: 500 Hz (felt) … 20 kHz (clear)
inline float attOf (float d)   { return .002f + (1 - d) * .016f; }                   // and a softer strike as it goes to felt
inline float relOf (float v)   { return .06f * std::pow (50.0f, v); }                // RELEASE: 60 ms … 3 s
inline float freeT (float v)   { return .03f + 1.97f * v * v; }                      // REVERSE TIME, free: 30 ms … 2 s
inline float swellOf (float v) { return .15f * std::pow (4 / .15f, v); }             // REVERSE SWELL: 0.15 … 4 s
inline float delT (float v)    { return .06f * std::pow (25.0f, v); }                // DELAY TIME, free: 60 ms … 1.5 s
inline float decOf (float v)   { return 1.5f * std::pow (16.0f, v); }                // REVERB SIZE: 1.5 s … 24 s of hall
inline float lvDb (float v)    { return -18 + 30 * v; }                              // PUNCH, TAIL LEVEL: -18 … +12 dB (0 dB at 0.6)
inline float lenOf (float v)   { return .008f * std::pow (20.0f, v); }               // STRIKE LENGTH: 8 … 160 ms
inline float warmCut (float v) { return 20000 * std::pow (700 / 20000.0f, v); }      // WARMTH: the tail's filter, open … 700 Hz
inline float plDec (float v)   { return .4f * std::pow (10.0f, v); }                 // PLATE DECAY 0.4 … 4 s
inline float plLow (float v)   { return 100 * std::pow (15.0f, v); }                 // PLATE LOW CUT 100 … 1500 Hz
inline float plHigh (float v)  { return 3000 * std::pow (6.0f, v); }                 // PLATE HIGH CUT 3 … 18 kHz
inline float dlvOf (float v)   { return v < .01f ? 0 : std::pow (v / .6f, 1.7f); }    // DRONE LEVEL: 0 dB at 0.6
constexpr float kDivBeats[3] = { 1, .75f, .5f };                                      // DELAY 1/4, 1/8·, 1/8 (in beats)
constexpr float kRevBeats[7] = { .25f, .5f, .75f, 1, 1.5f, 2, 4 };                    // REVERSE TIME in beats: 1/16 … 1 BAR

} // namespace horizon
