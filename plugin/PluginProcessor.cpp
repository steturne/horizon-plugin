#include "PluginProcessor.h"
#include "PluginEditor.h"

/* the knobs a DAW can automate: their IDs stay the same for ever (songs remember them by ID) */
juce::AudioProcessorValueTreeState::ParameterLayout HorizonProcessor::layout()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    ps.push_back (std::make_unique<P> (juce::ParameterID { "level", 1 }, "Level", juce::NormalisableRange<float> (-48.0f, 6.0f, 0.1f, 2.0f), 0.0f,
        A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return v <= -47.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; })));
    ps.push_back (std::make_unique<P> (juce::ParameterID { "hall", 1 }, "Hall", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f,
        A().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; })
           .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; })));
    ps.push_back (std::make_unique<P> (juce::ParameterID { "release", 1 }, "Release", juce::NormalisableRange<float> (0.06f, 3.0f, 0.001f, 0.4f), 0.35f,
        A().withLabel ("s").withStringFromValueFunction ([] (float v, int) { return v < 1 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s"; })));
    return { ps.begin(), ps.end() };
}

HorizonProcessor::HorizonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "HORIZON", layout()),
      library (SampleLibrary::get())
{
    level = state.getRawParameterValue ("level");
    hall = state.getRawParameterValue ("hall");
    release = state.getRawParameterValue ("release");
}

HorizonProcessor::~HorizonProcessor() = default;

void HorizonProcessor::prepareToPlay (double sampleRate, int) { engine.prepare (sampleRate); }

bool HorizonProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HorizonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (! engine.ready()) if (auto* s = library->set()) engine.setSamples (s);
    engine.setParams ({ level->load(), hall->load(), release->load() });

    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float scratch[512];
    /* the block is played in pieces between MIDI events, so each note starts on its own sample */
    int pos = 0;
    auto renderTo = [&] (int end) {
        while (pos < end)
        {
            const int k = std::min (end - pos, 256);
            float* r = R ? R + pos : scratch + 256;
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
        else if (msg.isSustainPedalOn()) engine.sustain (true);
        else if (msg.isSustainPedalOff()) engine.sustain (false);
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
