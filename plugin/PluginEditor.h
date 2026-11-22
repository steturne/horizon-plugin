/* HORIZON's window in the DAW: a web page (face/: the night sky over Steve's mountains, its aurora following how loud it plays,
   and HORIZON's whole console on clear glass), shown by JUCE 8's WebBrowserComponent (WebView2 on Windows, WKWebView on the Mac).
   The page's files are built into the plugin. Every control on the face is joined both ways to its plugin parameter by a JUCE
   relay (a knob by a WebSliderRelay, a switch by a WebToggleButtonRelay, a mode by a WebComboBoxRelay): turn it on the face and the
   DAW sees (and can record) the move; automate it in the DAW and the face follows. */
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class HorizonEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HorizonEditor (HorizonProcessor&);
    ~HorizonEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    int sizeTo (int width);   /* the face's − + and corner grip */

private:
    void timerCallback() override;
    static std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& url);

    /* one relay for each parameter (made before the browser: its options are made from them) */
    struct Relays
    {
        Relays();
        std::vector<std::unique_ptr<juce::WebSliderRelay>> sliders;
        std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggles;
        std::vector<std::unique_ptr<juce::WebComboBoxRelay>> combos;
    };

    HorizonProcessor& proc;
    Relays relays;
    juce::WebBrowserComponent web;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttach;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttach;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttach;
    int lastHits = 0;
    bool sized = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HorizonEditor)
};
