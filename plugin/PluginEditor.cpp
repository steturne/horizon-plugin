#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>

static juce::WebBrowserComponent::Options webOptions (juce::WebSliderRelay& a, juce::WebSliderRelay& b, juce::WebSliderRelay& c,
                                                      juce::WebBrowserComponent::ResourceProvider provider)
{
    using O = juce::WebBrowserComponent::Options;
    return O {}
        .withBackend (O::Backend::webview2)
        .withWinWebView2Options (O::WinWebView2 {}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("HORIZON-WebView"))
                                     .withBackgroundColour (juce::Colour (0xff05080f)))
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withOptionsFrom (a)
        .withOptionsFrom (b)
        .withOptionsFrom (c)
        .withResourceProvider (std::move (provider));
}

HorizonEditor::HorizonEditor (HorizonProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (webOptions (levelRelay, hallRelay, releaseRelay, [] (const juce::String& url) { return resource (url); })),
      levelAttach (*p.state.getParameter ("level"), levelRelay, nullptr),
      hallAttach (*p.state.getParameter ("hall"), hallRelay, nullptr),
      releaseAttach (*p.state.getParameter ("release"), releaseRelay, nullptr)
{
    addAndMakeVisible (web);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    /* 16:9, as HORIZON's face; any size from small to full HD */
    setResizable (true, true);
    setResizeLimits (640, 360, 1920, 1080);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (16.0 / 9.0);
    setSize (960, 540);
    startTimerHz (30);
}

HorizonEditor::~HorizonEditor() { stopTimer(); }

void HorizonEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff05080f)); }

void HorizonEditor::resized() { web.setBounds (getLocalBounds()); }

/* 30 times a second: how loud it plays (the sky's aurora follows it), a key struck (its velocity: a pulse, now and then a surge),
   the pedal, and while the piano loads, how far it has got */
void HorizonEditor::timerCallback()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("level", proc.engine.level());
    o->setProperty ("pedal", proc.engine.pedalDown());
    const int h = proc.engine.hits();
    if (h != lastHits) { lastHits = h; o->setProperty ("hit", proc.engine.lastHitVelocity()); }
    const auto s = proc.library->status();
    if (s != lastStatus) { lastStatus = s; o->setProperty ("status", s); }
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
