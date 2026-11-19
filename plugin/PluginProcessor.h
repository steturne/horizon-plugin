/* HORIZON as a plugin: MIDI in, stereo out. The sound is horizon::Engine (core/, no JUCE); this file only joins it to the DAW:
   the three knobs as automatable parameters (LEVEL, HALL, RELEASE), MIDI notes and the sustain pedal, and saving with the song. */
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/HorizonEngine.h"
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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "HORIZON"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    horizon::Engine engine;
    std::shared_ptr<SampleLibrary> library;

private:
    std::atomic<float>* level = nullptr;
    std::atomic<float>* hall = nullptr;
    std::atomic<float>* release = nullptr;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HorizonProcessor)
};
