/* HORIZON as a plugin: MIDI in, stereo out. The sound is horizon::Engine (core/, no JUCE); this file only joins it to the DAW:
   every HORIZON control as an automatable parameter (Params.h), MIDI notes and the sustain pedal, the DAW's tempo (for the DELAY
   and REVERSE in beats), and saving with the song. */
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <string>
#include "../core/HorizonEngine.h"
#include "Params.h"
#include "SampleLibrary.h"

class HorizonProcessor : public juce::AudioProcessor
{
public:
    HorizonProcessor();
    ~HorizonProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HORIZON"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    /* HORIZON's presets (Presets.h) are the plugin's programs: the DAW's preset list and the face's ‹ › */
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}
    std::atomic<int> currentProgram { 0 };

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    horizon::Engine engine;
    std::shared_ptr<SampleLibrary> library;
    std::shared_ptr<PadLibrary> padLibrary;
    std::atomic<bool> panicNow { false };            // PANIC on the face: every note stopped on the next block

    /* a parameter's value as text for the DAW (the same words HORIZON's face shows) */
    juce::String textOf (const juce::String& id, float v) const;
    float raw (const char* id) const;

private:
    std::map<std::string, std::atomic<float>*, std::less<>> rawById;   // (looked up by plain text: nothing made on the audio thread)
    void fill (horizon::Settings&) const;
    juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HorizonProcessor)
};
