#include "SampleLibrary.h"
#include <mutex>

std::shared_ptr<SampleLibrary> SampleLibrary::get()
{
    static std::mutex m;
    static std::weak_ptr<SampleLibrary> shared;
    std::lock_guard<std::mutex> g (m);
    auto p = shared.lock();
    if (! p) { p = std::shared_ptr<SampleLibrary> (new SampleLibrary()); shared = p; }
    return p;
}

SampleLibrary::SampleLibrary() : juce::Thread ("HORIZON samples") { startThread (juce::Thread::Priority::low); }

SampleLibrary::~SampleLibrary() { stopThread (8000); }

/* where the recordings are, next to the plugin's own program file (the plugin, not the DAW: JUCE finds the module it is in) */
juce::File SampleLibrary::findFolder()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile), dir = exe.getParentDirectory();
    for (auto f : { dir.getSiblingFile ("Resources").getChildFile ("Salamander Grand"),   // inside a bundle: Contents/Resources
                    dir.getChildFile ("Salamander Grand"),                                // beside a plain HORIZON.exe
                    dir.getChildFile ("Resources").getChildFile ("Salamander Grand") })
        if (f.isDirectory()) return f;
    return {};
}

/* a name like D#3v11.mp3 → the MIDI note (51) and layer (11); false if it is not one */
static bool parseName (const juce::String& name, int& note, int& layer)
{
    static const int semi[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    auto s = name.toUpperCase();
    if (! s.endsWith (".MP3") || s.length() < 7) return false;
    const auto c = s[0];
    if (c < 'A' || c > 'G') return false;
    int i = 1, sharp = 0;
    if (s[i] == '#') { sharp = 1; ++i; }
    if (! juce::CharacterFunctions::isDigit (s[i])) return false;
    const int octave = s[i] - '0'; ++i;
    if (s[i] != 'V') return false;
    layer = s.substring (i + 1, s.length() - 4).getIntValue();
    note = 12 * (octave + 1) + semi[c - 'A'] + sharp;
    return layer > 0;
}

void SampleLibrary::run()
{
    const auto folder = findFolder();
    juce::Array<juce::File> files;
    if (folder.isDirectory()) files = folder.findChildFiles (juce::File::findFiles, false, "*.mp3");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto set = std::make_unique<horizon::SampleSet>();
    int done = 0;
    for (auto& f : files)
    {
        if (threadShouldExit()) return;
        int note = 0, layer = 0, li = -1;
        if (parseName (f.getFileName(), note, layer))
            for (int k = 0; k < 4; ++k) if (horizon::kLayers[k] == layer) li = k;
        if (li >= 0)
            if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor (f) })
            {
                const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 12));
                juce::AudioBuffer<float> b ((int) juce::jmax (1u, r->numChannels), juce::jmax (1, n));
                b.clear();
                r->read (&b, 0, n, 0, true, true);
                set->add (note, li, b.getReadPointer (0), b.getNumChannels() > 1 ? b.getReadPointer (1) : nullptr, n, r->sampleRate);
            }
        if (++done % 8 == 0) setStatus ("TUNING... " + juce::String (done * 100 / juce::jmax (1, files.size())) + "%");
    }
    if (set->count() == 0) { setStatus ("NO PIANO SAMPLES"); return; }
    set->finish();                                   // how the layers of each key line up (measured once)
    owned = std::move (set);
    ready.store (owned.get(), std::memory_order_release);
    setStatus ({});
}
