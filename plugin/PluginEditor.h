/* HORIZON's window in the DAW: a web page (face/: the night sky over Steve's mountains, its aurora following how loud it plays,
   and the knobs on clear glass), shown by JUCE 8's WebBrowserComponent (WebView2 on Windows, WKWebView on the Mac). The page's
   files are built into the plugin. Each knob is joined both ways to its plugin parameter by a WebSliderRelay: turn it on the face
   and the DAW sees (and can record) the move; automate it in the DAW and the knob on the face turns. */
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

private:
    void timerCallback() override;
    static std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& url);

    HorizonProcessor& proc;
    /* (the relays first: the browser's options are made from them) */
    juce::WebSliderRelay levelRelay { "level" }, hallRelay { "hall" }, releaseRelay { "release" };
    juce::WebBrowserComponent web;
    juce::WebSliderParameterAttachment levelAttach, hallAttach, releaseAttach;
    int lastHits = 0;
    juce::String lastStatus { "-" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HorizonEditor)
};
