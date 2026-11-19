/* The piano recordings for the plugin: found inside the plugin itself (HORIZON.component / HORIZON.vst3 /
   HORIZON.app: Contents/Resources/Salamander Grand; the Windows stand-alone: a "Salamander Grand" folder beside HORIZON.exe),
   decoded once on a background thread, and shared by every HORIZON open in the same DAW (one copy in memory, ~130 MB). */
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../core/HorizonEngine.h"

class SampleLibrary : private juce::Thread
{
public:
    /* the one shared library (made, and its loading started, by the first HORIZON that asks) */
    static std::shared_ptr<SampleLibrary> get();
    ~SampleLibrary() override;

    /* the finished recordings, or null while loading (or if they were not found) */
    const horizon::SampleSet* set() const { return ready.load (std::memory_order_acquire); }
    /* for the face: "TUNING… 40%", "NO PIANO SAMPLES", or "" when ready */
    juce::String status() const { const juce::ScopedLock l (lock); return message; }
    static juce::File findFolder();

private:
    SampleLibrary();
    void run() override;
    void setStatus (const juce::String& s) { const juce::ScopedLock l (lock); message = s; }

    std::unique_ptr<horizon::SampleSet> owned;
    std::atomic<const horizon::SampleSet*> ready { nullptr };
    juce::CriticalSection lock;
    juce::String message { "TUNING..." };
};
