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
static juce::WebBrowserComponent::Options webOptions (HorizonProcessor& proc, HorizonEditor* ed, std::vector<std::unique_ptr<juce::WebSliderRelay>>& s,
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
        .withNativeFunction ("preset", [&proc] (const juce::Array<juce::var>& a, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {   /* the face's ‹ ›: the next or the one before */
                                 const int n = proc.getNumPrograms(), d = a.size() > 0 ? (int) a[0] : 1;
                                 proc.setCurrentProgram (((proc.getCurrentProgram() + d) % n + n) % n);
                                 done (juce::var (proc.getProgramName (proc.getCurrentProgram()))); })
        .withNativeFunction ("size", [ed] (const juce::Array<juce::var>& a, juce::WebBrowserComponent::NativeFunctionCompletion done)
                             {   /* the face's − + and its corner grip: the window this wide (Live on the Mac has no way of its own) */
                                 done (juce::var (ed->sizeTo (a.size() > 0 ? (int) a[0] : 1200))); })
        .withResourceProvider (std::move (provider));
    for (auto& r : s) o = o.withOptionsFrom (*r);
    for (auto& r : t) o = o.withOptionsFrom (*r);
    for (auto& r : c) o = o.withOptionsFrom (*r);
    return o;
}

HorizonEditor::HorizonEditor (HorizonProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (webOptions (p, this, relays.sliders, relays.toggles, relays.combos, [] (const juce::String& url) { return resource (url); }))
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
    /* 16:9, as HORIZON's face. The size the window was last left at (kept with the song); the first time, big: 85% of the
       screen's width (Steve found the first size a bit small on his MacBook), as long as it fits the screen's height.
       Read before setResizeLimits: that already sizes the window (to the smallest, 800 x 450) and resized() would keep it, which
       is what made every window open small. New names ("winW"), so songs saved with that small size open big again. */
    int w = (int) p.state.state.getProperty ("winW", 0), h = (int) p.state.state.getProperty ("winH", 0);
    setResizable (true, false);   /* no JUCE corner: the page covers it; the face has its own (and − +) */
    setResizeLimits (800, 450, 2560, 1440);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (16.0 / 9.0);
    if (w < 800 || h < 450)
    {
        juce::Rectangle<int> area (0, 0, 1440, 900);
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) area = d->userArea;
        w = juce::jmin (2560, (int) (area.getWidth() * .85)); h = w * 9 / 16;
        const int maxH = (int) (area.getHeight() * .85);
        if (h > maxH) { h = maxH; w = h * 16 / 9; }
        w = juce::jmax (800, w); h = juce::jmax (450, h);
    }
    setSize (w, h);
    sized = true;
    startTimerHz (30);
}

/* the window this wide (16:9), no smaller than 800 and no bigger than the screen it is on */
int HorizonEditor::sizeTo (int w)
{
    juce::Rectangle<int> area (0, 0, 2560, 1440);
    if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) area = d->userArea;
    const int most = juce::jmax (800, juce::jmin (2560, area.getWidth() - 20, (area.getHeight() - 50) * 16 / 9));   /* room for the DAW's title bar */
    w = juce::jlimit (800, most, w);
    setSize (w, w * 9 / 16);
    return getWidth();
}

HorizonEditor::~HorizonEditor() { stopTimer(); }

void HorizonEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff05080f)); }

void HorizonEditor::resized()
{
    web.setBounds (getLocalBounds());
    /* remembered with the song, so the window opens at the size it was left (not while it is still being made) */
    if (! sized) return;
    proc.state.state.setProperty ("winW", getWidth(), nullptr);
    proc.state.state.setProperty ("winH", getHeight(), nullptr);
}

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
    o->setProperty ("preset", proc.getProgramName (proc.getCurrentProgram()));
    o->setProperty ("pi", proc.getCurrentProgram());
    o->setProperty ("pn", proc.getNumPrograms());
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
