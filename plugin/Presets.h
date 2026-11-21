/* HORIZON's presets (mixtape2\Devices\horizon.js PRESETS), as the plugin's programs: each is every control at where it starts,
   then the preset's own settings over it. A preset with a PAD sound brings HALO's sound (its PRESETS, without its drone) and the
   PAD's own settings. "Dry piano" first: what a new HORIZON opens with in Mixtape. */
#pragma once
#include <vector>
#include <utility>

namespace hzp {

struct PadSound { const char* name; unsigned chars; float glassLv, airLv, stringsLv; int voicing, motion; float tone, width, swell, amt, space, tex, speed, sub, air, spark; };
/* HALO's sounds (chars: bits WARM 1 GLASS 2 CHOIR 4 STRINGS 8 AIR 16 ORGAN 32 BOWL 64 SHRUTI 128); voicing SINGLE OCTAVE FIFTH WIDE SUS; motion STILL BREATHE SHIMMER DRIFT PULSE WANDER */
static const PadSound kPadSounds[14] = {
    { "Warm blanket",      1,   1, 1, 1,  0, 1, .45f, .65f, .55f, .35f, .55f, .25f, .75f, .5f,  0, 0 },
    { "Glass cathedral",   2,   1, 1, 1,  1, 2, .7f,  .7f,  .6f,  .5f,  .85f, .05f, .75f, 0,    0, .3f },
    { "Night choir",       4,   1, 1, 1,  2, 3, .5f,  .6f,  .7f,  .4f,  .75f, .1f,  .75f, .15f, 0, 0 },
    { "Cinematic strings", 8,   1, 1, 1,  3, 1, .55f, .85f, .65f, .3f,  .7f,  .15f, .75f, .2f,  0, 0 },
    { "Ocean air",         16,  1, 1, 1,  1, 3, .6f,  .9f,  .8f,  .6f,  .8f,  .2f,  .75f, .1f,  0, 0 },
    { "Pulse ribbon",      1,   1, 1, 1,  2, 4, .6f,  .6f,  .3f,  .7f,  .55f, .2f,  .75f, .3f,  0, 0 },
    { "Dream sus",         2,   1, 1, 1,  4, 3, .62f, .75f, .7f,  .45f, .8f,  .1f,  .75f, 0,    0, 0 },
    { "Lo-fi tape pad",    1,   1, 1, 1,  1, 3, .35f, .5f,  .5f,  .55f, .45f, .7f,  .75f, .3f,  0, 0 },
    { "Glass choir",       6,   .8f, 1, 1, 1, 1, .6f, .8f,  .7f,  .35f, .8f,  .08f, .75f, .1f,  0, 0 },
    { "Harp drone",        128, 1, 1, 1,  0, 5, .5f,  .7f,  .8f,  .45f, .7f,  .15f, .5f,  .3f,  0, 0 },
    { "Singing bowl",      80,  1, .35f, 1, 0, 1, .6f, .75f, .75f, .3f,  .85f, .05f, .45f, 0,    0, 0 },
    { "Deep space",        18,  .5f, 1, 1, 0, 3, .4f, 1,    .9f,  .6f,  .95f, .2f,  .35f, .45f, 0, 0 },
    { "Cathedral organ",   32,  1, 1, 1,  1, 0, .55f, .6f,  .35f, .2f,  .9f,  .05f, .75f, .3f,  0, 0 },
    { "Monastery",         12,  1, 1, .5f, 0, 3, .45f, .7f, .85f, .35f, .85f, .1f,  .4f,  .35f, 0, 0 } };

struct Preset { const char* name; std::vector<std::pair<const char*, float>> set; int pad = -1; std::vector<std::pair<const char*, float>> padSet = {}; };

/* the preset's own settings: knobs 0 … 1, switches 0 / 1, modes by number (rev_kind: 0 BACK 1 P+B 2 ECHO; del_div: 0 1/4 1/8· 1/8) */
static const Preset kPresets[] = {
    { "Dry piano", { { "damp", .95f }, { "release", .22f }, { "verb_on", 0 } } },
    { "Sharp & warm", { { "damp", .9f }, { "release", .3f }, { "shape_punch", .7f }, { "shape_bright", .62f }, { "shape_len", .45f }, { "shape_hammer", .85f },
        { "shape_soft", .7f }, { "shape_warmth", .38f }, { "shape_darken", .25f }, { "shape_sustain", .45f }, { "shape_delS", .6f }, { "shape_revS", .3f },
        { "shape_tubeT", .7f }, { "shape_delT", .2f }, { "verb_mix", .35f } } },
    { "Endless felt", { { "damp", .35f }, { "release", .45f }, { "shape_punch", .45f }, { "shape_len", .6f }, { "shape_soft", .8f }, { "shape_warmth", .3f },
        { "shape_darken", .15f }, { "shape_sustain", 1 }, { "shape_sparkle", .2f }, { "tube_on", 1 }, { "tube_drive", .2f }, { "tube_warmth", .7f }, { "tube_level", .55f },
        { "verb_mix", .5f }, { "verb_size", .8f }, { "verb_shim", .2f }, { "verb_bloom", .2f } }, 0, { { "pad_level", .35f }, { "pad_rev", .3f } } },
    /* Steve's idea: the tube and the echoes on the hammer's strike, the hall on the ringing tail */
    { "Echo hammer", { { "damp", .85f }, { "release", .3f }, { "shape_punch", .68f }, { "shape_bright", .6f }, { "shape_hammer", .6f }, { "shape_soft", .4f },
        { "shape_warmth", .2f }, { "shape_sustain", .3f }, { "shape_revS", .15f }, { "shape_tubeT", .15f }, { "shape_delT", .08f },
        { "tube_on", 1 }, { "tube_drive", .4f }, { "tube_warmth", .55f }, { "del_on", 1 }, { "del_rep", .38f }, { "del_tone", .45f }, { "del_mix", .32f },
        { "verb_mix", .5f }, { "verb_size", .75f }, { "verb_shim", .1f }, { "verb_bloom", .1f } } },
    { "Meditate", { { "damp", .85f }, { "rev_on", 1 }, { "rev_time", .42f }, { "rev_swell", .5f }, { "rev_mix", .55f },
        { "verb_mix", .45f }, { "verb_size", .65f }, { "verb_shim", .12f }, { "verb_bloom", .05f } } },
    { "Felt", { { "damp", .18f }, { "release", .32f }, { "tube_on", 1 }, { "tube_drive", .25f }, { "tube_warmth", .75f }, { "tube_level", .55f },
        { "verb_mix", .35f }, { "verb_size", .5f } } },
    { "Sunday hall", { { "damp", .65f }, { "verb_mix", .5f }, { "verb_size", .78f }, { "verb_bloom", .12f } } },
    { "Shimmer prayer", { { "damp", .5f }, { "release", .35f }, { "rev_on", 1 }, { "rev_time", .5f }, { "rev_swell", .6f }, { "rev_mix", .4f },
        { "del_on", 1 }, { "del_rep", .35f }, { "del_tone", .4f }, { "del_mix", .18f }, { "verb_mix", .65f }, { "verb_size", .9f }, { "verb_shim", .55f },
        { "verb_bloom", .3f }, { "shape_sparkle", .35f } }, 1, { { "pad_level", .45f }, { "pad_rev", .35f } } },
    { "Backwards dream", { { "damp", .6f }, { "rev_on", 1 }, { "rev_kind", 0 }, { "rev_time", .1f }, { "rev_swell", .55f }, { "rev_mix", .8f },
        { "verb_mix", .6f }, { "verb_size", .85f }, { "verb_shim", .3f }, { "verb_bloom", .2f } } },
    { "Reverse echoes", { { "damp", .7f }, { "rev_on", 1 }, { "rev_kind", 2 }, { "rev_time", .5f }, { "rev_swell", .4f }, { "rev_rep", .55f }, { "rev_mix", .55f },
        { "rev_sync", 1 }, { "verb_mix", .5f }, { "verb_size", .7f }, { "verb_shim", .1f } } },
    { "Warm tape", { { "damp", .4f }, { "tube_on", 1 }, { "tube_drive", .55f }, { "tube_warmth", .6f }, { "del_on", 1 }, { "del_rep", .4f }, { "del_tone", .3f },
        { "del_mix", .22f }, { "del_div", 0 }, { "verb_mix", .3f }, { "verb_size", .45f } } },
    { "Plain grand", { { "damp", .9f }, { "release", .2f }, { "verb_mix", .2f }, { "verb_size", .4f } } },
    /* for prayer and the moments between songs: the piano soft and far, a choir pad on the left hand, a drone of root and fifth in C */
    { "Prayer drone", { { "damp", .55f }, { "release", .4f }, { "shape_sparkle", .25f }, { "shape_sustain", .4f }, { "verb_mix", .6f }, { "verb_size", .85f },
        { "verb_shim", .3f }, { "verb_bloom", .25f } }, 13, { { "pad_level", .45f }, { "pad_rev", .3f }, { "pad_range", 1 }, { "dr_root", 1 }, { "dr_fifth", 1 } } },
    { "Pad and piano", { { "damp", .8f }, { "release", .3f }, { "verb_mix", .45f }, { "verb_size", .7f }, { "verb_shim", .15f }, { "verb_bloom", .1f } }, 3,
        { { "pad_level", .45f }, { "pad_rev", .2f } } },
};
static const int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

} // namespace hzp
