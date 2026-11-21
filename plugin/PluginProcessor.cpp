#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace horizon;

/* every control from the table (Params.h); version 2: the first test version's LEVEL / HALL / RELEASE meant other things */
juce::AudioProcessorValueTreeState::ParameterLayout HorizonProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    for (const auto& d : hzp::kParams)
    {
        const juce::ParameterID pid { d.id, 2 };
        const juce::String id (d.id);
        if (d.kind == hzp::knob)
            ps.push_back (std::make_unique<juce::AudioParameterFloat> (pid, d.name, juce::NormalisableRange<float> (0, 1), d.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([this, id] (float v, int) { return textOf (id, v); })));
        else if (d.kind == hzp::toggle)
            ps.push_back (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > .5f));
        else
            ps.push_back (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
    }
    return { ps.begin(), ps.end() };
}

HorizonProcessor::HorizonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "HORIZON", layout()),
      library (SampleLibrary::get())
{
    for (const auto& d : hzp::kParams) rawById[d.id] = state.getRawParameterValue (d.id);
}

HorizonProcessor::~HorizonProcessor() = default;

float HorizonProcessor::raw (const char* id) const
{
    const auto it = rawById.find (id);
    return it != rawById.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

/* the parameters into HORIZON's settings (as horizon.js keeps them) */
void HorizonProcessor::fill (Settings& s) const
{
    auto k = [this] (const char* id) { return raw (id); };
    auto b = [this] (const char* id) { return raw (id) > .5f; };
    auto c = [this] (const char* id) { return (int) std::lround (raw (id)); };
    s.piano = c ("piano"); s.level = k ("level"); s.damp = k ("damp"); s.release = k ("release");
    auto& sh = s.shape;
    sh.punch = k ("shape_punch"); sh.bright = k ("shape_bright"); sh.len = k ("shape_len"); sh.hammer = k ("shape_hammer"); sh.tail = k ("shape_tail");
    sh.soft = k ("shape_soft"); sh.warmth = k ("shape_warmth"); sh.darken = k ("shape_darken"); sh.sustain = k ("shape_sustain"); sh.sparkle = k ("shape_sparkle");
    sh.tubeS = k ("shape_tubeS"); sh.delS = k ("shape_delS"); sh.revS = k ("shape_revS"); sh.tubeT = k ("shape_tubeT"); sh.delT = k ("shape_delT"); sh.revT = k ("shape_revT");
    s.rev = { b ("rev_on"), c ("rev_kind"), k ("rev_time"), k ("rev_swell"), k ("rev_rep"), k ("rev_mix"), b ("rev_sync"), b ("rev_rings") };
    s.tube = { b ("tube_on"), k ("tube_drive"), k ("tube_warmth"), k ("tube_level") };
    s.tape = { b ("tape_on"), k ("tape_wobble"), k ("tape_sat"), k ("tape_hiss") };
    s.del = { b ("del_on"), k ("del_time"), k ("del_rep"), k ("del_tone"), k ("del_mix"), b ("del_sync"), c ("del_div"), k ("del_duck") };
    s.verb = { b ("verb_on"), k ("verb_mix"), k ("verb_size"), k ("verb_shim"), k ("verb_bloom"), b ("verb_freeze"), k ("verb_pre"), k ("verb_low"), k ("verb_duck") };
    s.plate = { b ("plate_on"), k ("plate_mix"), k ("plate_decay"), k ("plate_pre"), k ("plate_low"), k ("plate_high"), k ("plate_duck") };
    auto& ch = s.chan;
    ch.low = k ("chan_low"); ch.body = k ("chan_body"); ch.pres = k ("chan_pres"); ch.air = k ("chan_air"); ch.glue = k ("chan_glue");
    ch.blend = k ("chan_blend"); ch.width = k ("chan_width"); ch.mono = b ("chan_mono"); ch.out = k ("chan_out");
    auto& p = s.pad;
    p.on = b ("pad_on"); p.level = k ("pad_level"); p.rev = k ("pad_rev"); p.low = k ("pad_low"); p.duck = k ("pad_duck"); p.dlevel = k ("pad_dlevel");
    static const char* const chId[8] = { "pad_ch_warm", "pad_ch_glass", "pad_ch_choir", "pad_ch_strings", "pad_ch_air", "pad_ch_organ", "pad_ch_bowl", "pad_ch_shruti" };
    static const char* const lvId[8] = { "pad_lv_warm", "pad_lv_glass", "pad_lv_choir", "pad_lv_strings", "pad_lv_air", "pad_lv_organ", "pad_lv_bowl", "pad_lv_shruti" };
    for (int i = 0; i < 8; ++i) { p.ch[i] = b (chId[i]); p.lv[i] = k (lvId[i]); }
    p.motion = c ("pad_motion"); p.voicing = c ("pad_voicing"); p.range = c ("pad_range"); p.oct = c ("pad_oct"); p.key = c ("dr_key");
    p.root = b ("dr_root"); p.fifth = b ("dr_fifth"); p.octave = b ("dr_oct"); p.sub = b ("dr_sub");
    p.swell = k ("pad_swell"); p.tone = k ("pad_tone"); p.width = k ("pad_width"); p.amt = k ("pad_amt"); p.speed = k ("pad_speed"); p.space = k ("pad_space");
    p.tex = k ("pad_tex"); p.subLv = k ("pad_sub"); p.air = k ("pad_air"); p.spark = k ("pad_spark");
}

/* HORIZON's own words for a knob's value (horizon.js fmt) */
juce::String HorizonProcessor::textOf (const juce::String& id, float v) const
{
    using juce::String;
    auto pct = [] (float x) { return String (juce::roundToInt (x * 100)) + "%"; };
    auto sec = [] (float s) { return s < 1 ? String (juce::roundToInt (s * 1000)) + " ms" : String (s, 2) + " s"; };
    auto sgn = [] (float d) { return (d >= 0 ? "+" : "") + String (d, 1) + " dB"; };
    auto hz = [] (float f) { return f >= 1000 ? String (f / 1000, 1) + " kHz" : String (juce::roundToInt (f)) + " Hz"; };
    if (id == "release") return sec (relOf (v));
    if (id == "damp") return v < .35f ? "FELT" : v > .75f ? "CLEAR" : pct (v);
    if (id == "rev_time")
    {
        static const char* beats[7] = { "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1 bar" };
        return raw ("rev_sync") > .5f ? String (beats[juce::jlimit (0, 6, juce::roundToInt (v * 6))]) : sec (freeT (v));
    }
    if (id == "rev_swell") return sec (swellOf (v));
    if (id == "del_time")
    {
        static const char* divs[3] = { "1/4", "1/8 dotted", "1/8" };
        return raw ("del_sync") > .5f ? String (divs[juce::jlimit (0, 2, (int) std::lround (raw ("del_div")))]) : sec (delT (v));
    }
    if (id == "verb_size") return String (decOf (v), 1) + " s";
    if (id == "shape_punch" || id == "shape_tail") return sgn (lvDb (v));
    if (id == "shape_bright") return sgn ((v - .5f) * 24);
    if (id == "shape_len") return String (juce::roundToInt (lenOf (v) * 1000)) + " ms";
    if (id == "shape_warmth") return v < .02f ? "OPEN" : hz (warmCut (v));
    if (id == "shape_sustain") return v < .01f ? "NATURAL" : v >= .97f ? "ENDLESS" : pct (v);
    if ((id == "shape_hammer" || id == "shape_soft" || id == "shape_darken" || id == "shape_sparkle") && v < .02f) return "OFF";
    if (id == "chan_out") return sgn (-30 + 36 * v);
    if (id == "chan_low") return v < .02f ? "OFF" : hz (30 * std::pow (12.0f, v));
    if (id == "chan_body" || id == "chan_pres" || id == "chan_air") return sgn ((v - .5f) * 18);
    if (id == "chan_width") return raw ("chan_mono") > .5f ? "MONO" : String (juce::roundToInt (v * 150)) + "%";
    if (id == "verb_pre") return String (juce::roundToInt (v * 200)) + " ms";
    if (id == "verb_low") return hz (40 * std::pow (15.0f, v));
    if (id == "pad_low") return v < .02f ? "OFF" : hz (30 * std::pow (13.0f, v));
    if (id == "pad_dlevel") return v < .01f ? "OFF" : sgn (20 * std::log10 (dlvOf (v)));
    if (id == "plate_decay") return String (plDec (v), 1) + " s";
    if (id == "plate_pre") return String (juce::roundToInt (v * 120)) + " ms";
    if (id == "plate_low") return hz (plLow (v));
    if (id == "plate_high") return hz (plHigh (v));
    if (id.endsWith ("duck") && v < .02f) return "OFF";
    return pct (v);
}

void HorizonProcessor::prepareToPlay (double sampleRate, int)
{
    Settings s; fill (s); engine.setSettings (s);
    engine.prepare (sampleRate);
}

bool HorizonProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HorizonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (! engine.ready()) if (auto* s = library->set()) engine.setSamples (s);
    Settings s; fill (s); engine.setSettings (s);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine.setTempo (*bpm);
    if (panicNow.exchange (false)) engine.allNotesOff();

    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float scratch[256];
    /* the block is played in pieces between MIDI events, so each note starts on its own sample */
    int pos = 0;
    auto renderTo = [&] (int end) {
        while (pos < end)
        {
            const int k = std::min (end - pos, 256);
            float* r = R ? R + pos : scratch;
            engine.render (L + pos, r, k);
            if (! R) for (int i = 0; i < k; ++i) L[pos + i] = 0.5f * (L[pos + i] + r[i]);
            pos += k;
        }
    };
    for (const auto m : midi)
    {
        renderTo (juce::jlimit (0, n, m.samplePosition));
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
        else if (msg.isSustainPedalOn()) engine.pedal (true);
        else if (msg.isSustainPedalOff()) engine.pedal (false);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    renderTo (n);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

juce::AudioProcessorEditor* HorizonProcessor::createEditor() { return new HorizonEditor (*this); }

void HorizonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void HorizonProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HorizonProcessor(); }
