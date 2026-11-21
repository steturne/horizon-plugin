#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>

HorizonEditor::Relays::Relays()
{
    for (const auto& d : hzp::kParams)
    {
        if (d.kind == hzp::knob) sliders.push_back (std::make_unique<juce::WebSliderRelay> (d.id));
        else if (d.kind == hzp::toggle) toggles.push_back (std::make_unique<juce::WebToggleButtonRelay> (d.id));
        else combos.push_back (std::make_unique<juce::WebComboBoxRelay> (d.id));
    }
}

/* the browser's options: every relay, PANIC (a function the page can call), the page's files */
static juce::WebBrowserComponent::Options webOptions (HorizonProcessor& proc, std::vector<std::unique_ptr<juce::WebSliderRelay>>& s,
                                                      std::vector<std::unique_ptr<juce::WebToggleButtonRelay>>& t,
                                                      std::vector<std::unique_ptr<juce::WebComboBoxRelay>>& c,
                                                      juce::WebBrowserComponent::ResourceProvider provider)
{
    using O = juce::WebBrowserComponent::Options;
    auto o = O {}
        .withBackend (O::Backend::webview2)
        .withWinWebView2Options (O::WinWebView2 {}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("HORIZON-WebView"))
                                     .withBackgroundColour (juce::Colour (0xff05080f)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeFunction ("panic", [&proc] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             { proc.panicNow = true; done (juce::var (true)); })
        .withResourceProvider (std::move (provider));
    for (auto& r : s) o = o.withOptionsFrom (*r);
    for (auto& r : t) o = o.withOptionsFrom (*r);
    for (auto& r : c) o = o.withOptionsFrom (*r);
    return o;
}

HorizonEditor::HorizonEditor (HorizonProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (webOptions (p, relays.sliders, relays.toggles, relays.combos, [] (const juce::String& url) { return resource (url); }))
{
    size_t si = 0, ti = 0, ci = 0;
    for (const auto& d : hzp::kParams)
    {
        auto& param = *p.state.getParameter (d.id);
        if (d.kind == hzp::knob) sliderAttach.push_back (std::make_unique<juce::WebSliderParameterAttachment> (param, *relays.sliders[si++], nullptr));
        else if (d.kind == hzp::toggle) toggleAttach.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (param, *relays.toggles[ti++], nullptr));
        else comboAttach.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (param, *relays.combos[ci++], nullptr));
    }
    addAndMakeVisible (web);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    /* 16:9, as HORIZON's face; any size from small to full HD */
    setResizable (true, true);
    setResizeLimits (800, 450, 1920, 1080);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (16.0 / 9.0);
    setSize (1120, 630);
    startTimerHz (30);
}

HorizonEditor::~HorizonEditor() { stopTimer(); }

void HorizonEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff05080f)); }

void HorizonEditor::resized() { web.setBounds (getLocalBounds()); }

/* 30 times a second: how loud it plays (the sky's aurora follows it), a key struck (its velocity: a pulse, now and then a surge),
   the pedal, and the piano's loading state (every time: a window opened, or a page still starting, after the piano was ready
   would otherwise never hear it and keep saying TUNING) */
void HorizonEditor::timerCallback()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("level", proc.engine.level());
    o->setProperty ("pedal", proc.engine.pedalDown());
    const int h = proc.engine.hits();
    if (h != lastHits) { lastHits = h; o->setProperty ("hit", proc.engine.lastHitVelocity()); }
    o->setProperty ("status", proc.library->status());
    web.emitEventIfBrowserIsVisible ("sky", juce::var (o));
}

/* the page's files, built into the plugin (BinaryData): "/" is horizon.html; anything else by its file name */
std::optional<juce::WebBrowserComponent::Resource> HorizonEditor::resource (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false);
    auto name = (path == "/" || path.isEmpty()) ? juce::String ("horizon.html") : path.fromLastOccurrenceOf ("/", false, false);
    static const std::pair<const char*, const char*> mimes[] = {
        { "html", "text/html" }, { "js", "text/javascript" }, { "css", "text/css" }, { "jpg", "image/jpeg" }, { "png", "image/png" }, { "json", "application/json" } };
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (name != BinaryData::originalFilenames[i]) continue;
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        if (data == nullptr) return std::nullopt;
        juce::String mime = "application/octet-stream";
        const auto ext = name.fromLastOccurrenceOf (".", false, false).toLowerCase();
        for (auto& m : mimes) if (ext == m.first) mime = m.second;
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);
        return juce::WebBrowserComponent::Resource { std::move (bytes), mime };
    }
    return std::nullopt;
}
