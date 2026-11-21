/* Every HORIZON control as a plugin parameter, so a DAW can automate it and saves it with the song. The IDs never change once
   released (songs remember them): each is HORIZON's own setting name (mixtape2\Devices\horizon.js) with its dots made underscores,
   and the PAD's sound (pad.P.x there) is pad_x. Knobs are 0 … 1 exactly as HORIZON keeps them; switches are on / off; modes are
   lists. The face (face/face.js) uses the same IDs. */
#pragma once

namespace hzp {

enum Kind { knob, toggle, choice };
struct Def { const char* id; const char* name; Kind kind; float def; const char* choices = nullptr; };

static const Def kParams[] = {
    /* the PIANO */
    { "piano", "Piano", choice, 0, "ON|FX ONLY|OFF" },
    { "level", "Piano Level", knob, .75f }, { "damp", "Damp", knob, .6f }, { "release", "Release", knob, .25f },
    /* the SHAPER: the strike and the tail */
    { "shape_punch", "Punch", knob, .6f }, { "shape_bright", "Bright", knob, .5f }, { "shape_len", "Strike Length", knob, .53f },
    { "shape_hammer", "Hammer", knob, 0 }, { "shape_tail", "Tail Level", knob, .6f }, { "shape_soft", "Soft Tail", knob, 0 },
    { "shape_warmth", "Warmth", knob, 0 }, { "shape_darken", "Darken", knob, 0 }, { "shape_sustain", "Sustain", knob, 0 },
    { "shape_sparkle", "Sparkle", knob, 0 },
    { "shape_tubeS", "Strike > Tube", knob, 1 }, { "shape_delS", "Strike > Delay", knob, 1 }, { "shape_revS", "Strike > Reverb", knob, 1 },
    { "shape_tubeT", "Tail > Tube", knob, 1 }, { "shape_delT", "Tail > Delay", knob, 1 }, { "shape_revT", "Tail > Reverb", knob, 1 },
    /* REVERSE */
    { "rev_on", "Reverse On", toggle, 0 }, { "rev_kind", "Reverse Mode", choice, 1, "BACK|P+B|ECHO" },
    { "rev_time", "Reverse Time", knob, .35f }, { "rev_swell", "Reverse Swell", knob, .45f }, { "rev_rep", "Reverse Repeats", knob, .45f },
    { "rev_mix", "Reverse Mix", knob, .5f }, { "rev_sync", "Reverse In Beats", toggle, 0 }, { "rev_rings", "Reverse Rings On", toggle, 1 },
    /* EFFECTS */
    { "tube_on", "Tube On", toggle, 0 }, { "tube_drive", "Tube Drive", knob, .3f }, { "tube_warmth", "Tube Warmth", knob, .5f }, { "tube_level", "Tube Level", knob, .5f },
    { "tape_on", "Tape On", toggle, 0 }, { "tape_wobble", "Tape Wobble", knob, .3f }, { "tape_sat", "Tape Saturation", knob, .3f }, { "tape_hiss", "Tape Hiss", knob, .1f },
    { "del_on", "Delay On", toggle, 0 }, { "del_sync", "Delay In Beats", toggle, 1 }, { "del_div", "Delay Beat", choice, 1, "1/4|1/8 DOTTED|1/8" },
    { "del_time", "Delay Time", knob, .45f }, { "del_rep", "Delay Repeats", knob, .3f }, { "del_tone", "Delay Tone", knob, .5f },
    { "del_mix", "Delay Mix", knob, .25f }, { "del_duck", "Delay Duck", knob, 0 },
    { "verb_on", "Reverb On", toggle, 1 }, { "verb_freeze", "Freeze", toggle, 0 }, { "verb_mix", "Reverb Mix", knob, .4f },
    { "verb_size", "Reverb Size", knob, .6f }, { "verb_shim", "Shimmer", knob, 0 }, { "verb_bloom", "Bloom", knob, 0 },
    { "verb_pre", "Pre-delay", knob, .12f }, { "verb_low", "Reverb Low Cut", knob, .35f }, { "verb_duck", "Reverb Duck", knob, 0 },
    /* OUT: the PLATE and the CHANNEL */
    { "plate_on", "Plate On", toggle, 0 }, { "plate_mix", "Plate Mix", knob, .35f }, { "plate_decay", "Plate Decay", knob, .5f },
    { "plate_pre", "Plate Pre-delay", knob, .15f }, { "plate_low", "Plate Low Cut", knob, .66f }, { "plate_high", "Plate High Cut", knob, .67f },
    { "plate_duck", "Plate Duck", knob, 0 },
    { "chan_low", "Low Cut", knob, 0 }, { "chan_body", "Body", knob, .5f }, { "chan_pres", "Presence", knob, .5f }, { "chan_air", "Air", knob, .5f },
    { "chan_glue", "Glue", knob, .25f }, { "chan_blend", "Glue Blend", knob, .5f }, { "chan_width", "Width", knob, .667f },
    { "chan_mono", "Mono", toggle, 0 }, { "chan_out", "Master", knob, .833f },
    /* the PAD (HALO's sound) and the DRONE */
    { "pad_on", "Pad On", toggle, 0 }, { "pad_preset", "Pad Sound", choice, 0,
      "Warm blanket|Glass cathedral|Night choir|Cinematic strings|Ocean air|Pulse ribbon|Dream sus|Lo-fi tape pad|Glass choir|Harp drone|Singing bowl|Deep space|Cathedral organ|Monastery" },
    { "pad_level", "Pad Level", knob, .5f }, { "pad_rev", "Pad Into Hall", knob, .25f }, { "pad_low", "Pad Low Cut", knob, 0 }, { "pad_duck", "Pad Duck", knob, 0 },
    { "pad_swell", "Pad Swell", knob, .55f }, { "pad_tone", "Pad Tone", knob, .45f }, { "pad_width", "Pad Width", knob, .65f }, { "pad_amt", "Pad Motion", knob, .35f },
    { "pad_speed", "Pad Speed", knob, .75f }, { "pad_space", "Pad Space", knob, .55f }, { "pad_tex", "Pad Texture", knob, .25f }, { "pad_sub", "Pad Sub", knob, .3f },
    { "pad_air", "Pad Air", knob, 0 }, { "pad_spark", "Pad Sparkle", knob, 0 },
    { "pad_motion", "Pad Motion Kind", choice, 1, "STILL|BREATHE|SHIMMER|DRIFT|PULSE|WANDER" },
    { "pad_voicing", "Pad Voicing", choice, 0, "SINGLE|OCTAVE|FIFTH|WIDE|SUS" },
    { "pad_range", "Pad Played By", choice, 0, "ALL KEYS|LEFT HAND" }, { "pad_oct", "Pad Octave", choice, 1, "OCT -|OCT 0|OCT +" },
    { "pad_ch_warm", "Pad Warm", toggle, 1 }, { "pad_ch_glass", "Pad Glass", toggle, 0 }, { "pad_ch_choir", "Pad Choir", toggle, 0 }, { "pad_ch_strings", "Pad Strings", toggle, 0 },
    { "pad_ch_air", "Pad Air Character", toggle, 0 }, { "pad_ch_organ", "Pad Organ", toggle, 0 }, { "pad_ch_bowl", "Pad Bowl", toggle, 0 }, { "pad_ch_shruti", "Pad Shruti", toggle, 0 },
    { "pad_lv_warm", "Pad Warm Level", knob, 1 }, { "pad_lv_glass", "Pad Glass Level", knob, 1 }, { "pad_lv_choir", "Pad Choir Level", knob, 1 }, { "pad_lv_strings", "Pad Strings Level", knob, 1 },
    { "pad_lv_air", "Pad Air Level", knob, 1 }, { "pad_lv_organ", "Pad Organ Level", knob, 1 }, { "pad_lv_bowl", "Pad Bowl Level", knob, 1 }, { "pad_lv_shruti", "Pad Shruti Level", knob, 1 },
    { "pad_dlevel", "Drone Level", knob, .6f }, { "dr_key", "Drone Key", choice, 0, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
    { "dr_root", "Drone Root", toggle, 0 }, { "dr_fifth", "Drone Fifth", toggle, 0 }, { "dr_oct", "Drone Octave", toggle, 0 }, { "dr_sub", "Drone Sub", toggle, 0 },
};
static constexpr int kNumParams = (int) (sizeof (kParams) / sizeof (kParams[0]));
static const char* const kChars[8] = { "warm", "glass", "choir", "strings", "air", "organ", "bowl", "shruti" };

} // namespace hzp
