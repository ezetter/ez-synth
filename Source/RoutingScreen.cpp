#include "RoutingScreen.h"

#include "SynthLookAndFeel.h"

namespace
{
    juce::StringArray getOscillatorNames()
    {
        juce::StringArray names;

        for (int i = 0; i < SynthProcessor::numOscillators; ++i)
            names.add ("Osc " + juce::String (i + 1));

        return names;
    }

    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    const juce::String arrow (juce::CharPointer_UTF8 ("  \xe2\x86\x92  "));
}

/** A plugin's own window, which closes when its plugin's removed. */
class RoutingScreen::PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioPluginInstance& pluginToShow, const juce::String& title, std::function<void (PluginWindow&)> closed)
        : DocumentWindow (title, colours::panel, juce::DocumentWindow::closeButton),
          plugin (pluginToShow),
          onClosed (std::move (closed))
    {
        setUsingNativeTitleBar (true);

        juce::AudioProcessorEditor* editor = plugin.createEditorAndMakeActive();

        if (editor == nullptr)
            editor = new juce::GenericAudioProcessorEditor (plugin);

        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~PluginWindow() override
    {
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        onClosed (*this);
    }

    juce::AudioPluginInstance& plugin;

private:
    std::function<void (PluginWindow&)> onClosed;
};

RoutingScreen::RoutingScreen (SynthProcessor& processor)
    : synthProcessor (processor),
      oscillatorButtons (getOscillatorNames())
{
    setWantsKeyboardFocus (true);

    doneButton.setMouseClickGrabsKeyboardFocus (false);
    doneButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (doneButton);

    oscillatorButtons.setTitle ("Oscillator");
    oscillatorButtons.onChoose = [this] (int chosen)
    {
        showOscillator (chosen);

        if (onOscillatorChosen != nullptr)
            onOscillatorChosen (chosen);
    };
    addAndMakeVisible (oscillatorButtons);

    for (auto& row : rows)
    {
        row.sendSwitch.setMouseClickGrabsKeyboardFocus (false);
        row.sendSwitch.setWantsKeyboardFocus (false);
        row.sendSwitch.onStateChange = [this] { repaint(); };
        addAndMakeVisible (row.sendSwitch);
    }

    // The effects page, which only the standalone app has.
    pageButtons.setTitle ("Routing page");
    pageButtons.onChoose = [this] (int chosen) { showPage (chosen == 1 ? Page::effects : Page::filters); };
    addChildComponent (pageButtons);
    pageButtons.setVisible (synthProcessor.hostsEffects());

    findPluginsButton.setMouseClickGrabsKeyboardFocus (false);
    findPluginsButton.setTooltip ("Look for the VST3 and Audio Unit effects installed on this Mac");
    findPluginsButton.onClick = [this] { findPlugins(); };
    addChildComponent (findPluginsButton);

    for (size_t bus = 0; bus < busRows.size(); ++bus)
    {
        auto& row = busRows[bus];
        row.sendSwitch.setMouseClickGrabsKeyboardFocus (false);
        row.sendSwitch.setWantsKeyboardFocus (false);
        row.sendSwitch.onStateChange = [this] { repaint(); };
        addChildComponent (row.sendSwitch);

        for (size_t slot = 0; slot < row.slots.size(); ++slot)
        {
            auto& button = row.slots[slot];
            button.setMouseClickGrabsKeyboardFocus (false);
            button.setWantsKeyboardFocus (false);
            button.onClick = [this, bus, slot] { showSlotMenu ((int) bus, (int) slot); };
            addChildComponent (button);
        }
    }

    // A plugin's window closes before the plugin goes.
    synthProcessor.getEffectRack().beforeRemoving = [this] (juce::AudioPluginInstance& plugin) { closePluginWindow (plugin); };

    showOscillator (0);
    showPage (Page::filters);
}

RoutingScreen::~RoutingScreen()
{
    synthProcessor.getEffectRack().beforeRemoving = nullptr;
    pluginWindows.clear();

    if (findPluginsWindow != nullptr)
        delete findPluginsWindow.getComponent();
}

void RoutingScreen::showPage (Page newPage)
{
    page = synthProcessor.hostsEffects() ? newPage : Page::filters;
    pageButtons.setChosen (page == Page::effects ? 1 : 0);
    const auto effects = page == Page::effects;

    for (auto& row : rows)
        row.sendSwitch.setVisible (! effects);

    findPluginsButton.setVisible (effects);

    for (auto& row : busRows)
    {
        row.sendSwitch.setVisible (effects);

        for (auto& button : row.slots)
            button.setVisible (effects);
    }

    showSlots();
    resized();
    repaint();
}

void RoutingScreen::showOscillator (int newOscillator)
{
    oscillator = juce::jlimit (0, SynthProcessor::numOscillators - 1, newOscillator);
    oscillatorButtons.setChosen (oscillator);

    for (size_t filter = 0; filter < rows.size(); ++filter)
    {
        auto& row = rows[filter];
        row.attachment.reset();
        row.sendSwitch.setTitle ("Osc " + juce::String (oscillator + 1) + " to filter " + juce::String ((int) filter + 1));
        row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            synthProcessor.parameters, SynthProcessor::filterSendId (oscillator, (int) filter), row.sendSwitch);
    }

    for (size_t bus = 0; bus < busRows.size(); ++bus)
    {
        auto& row = busRows[bus];
        row.attachment.reset();
        row.sendSwitch.setTitle ("Osc " + juce::String (oscillator + 1) + " to bus " + juce::String ((int) bus + 1));
        row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            synthProcessor.parameters, SynthProcessor::busSendId (oscillator, (int) bus), row.sendSwitch);
    }

    refresh();
    repaint();
}

juce::String RoutingScreen::describeFilter (int filter) const
{
    const auto ids = SynthProcessor::filterIds (filter);
    const auto on = synthProcessor.parameters.getRawParameterValue (ids.on)->load() >= 0.5f;
    const auto type = (FilterType) juce::roundToInt (synthProcessor.parameters.getRawParameterValue (ids.type)->load());

    if (! on)
        return FilterStage::getName (type) + dot + "switched out, so nothing's filtered by it";

    return FilterStage::getName (type) + dot + synthProcessor.parameters.getParameter (ids.cutoff)->getCurrentValueAsText();
}

void RoutingScreen::refresh()
{
    auto changed = false;

    for (size_t filter = 0; filter < rows.size(); ++filter)
    {
        const auto settings = describeFilter ((int) filter);
        changed = changed || settings != rows[filter].shownSettings;
        rows[filter].shownSettings = settings;
    }

    if (synthProcessor.getEffectRack().getVersion() != shownRackVersion)
    {
        showSlots();
        changed = true;
    }

    if (changed)
        repaint();
}

void RoutingScreen::showSlots()
{
    auto& rack = synthProcessor.getEffectRack();
    shownRackVersion = rack.getVersion();

    for (int bus = 0; bus < EffectRack::numBuses; ++bus)
    {
        for (int slot = 0; slot < EffectRack::slotsPerBus; ++slot)
        {
            auto& button = getSlotButton (bus, slot);
            const auto info = rack.getSlot (bus, slot);
            button.setButtonText (info.missing    ? "Missing: " + info.name
                                  : info.name.isEmpty() ? juce::String ("+")
                                  : info.bypassed ? info.name + " (off)"
                                                  : info.name);
            button.setColour (juce::TextButton::textColourOffId, info.missing    ? colours::accent
                                                                : info.bypassed || ! info.loaded ? colours::dimText
                                                                                 : colours::text);
            button.setTooltip (info.missing ? info.name + " is saved with this sound, but isn't on this Mac"
                               : info.loaded ? "Bus " + juce::String (bus + 1) + ", slot " + juce::String (slot + 1) + ": " + info.name
                                             : "Load an effect into bus " + juce::String (bus + 1) + ", slot " + juce::String (slot + 1));
        }
    }
}

void RoutingScreen::showSlotMenu (int bus, int slot)
{
    auto& rack = synthProcessor.getEffectRack();
    const auto info = rack.getSlot (bus, slot);
    const auto effects = rack.getEffects();

    juce::PopupMenu load;
    juce::String maker;
    juce::PopupMenu byMaker;

    // The effects found, by manufacturer.
    for (int i = 0; i < effects.size(); ++i)
    {
        const auto& effect = effects.getReference (i);
        const auto name = effect.manufacturerName.isNotEmpty() ? effect.manufacturerName : juce::String ("Other");

        if (name != maker && maker.isNotEmpty())
        {
            load.addSubMenu (maker, byMaker);
            byMaker = {};
        }

        maker = name;
        byMaker.addItem (1000 + i, effect.name + "  (" + effect.pluginFormatName + ")");
    }

    if (maker.isNotEmpty())
        load.addSubMenu (maker, byMaker);
    else
        load.addItem (-1, "No effects found yet: click Find Plugins", false);

    juce::PopupMenu menu;
    enum { showWindow = 1, bypass, removeIt };

    if (info.missing)
        menu.addSectionHeader (info.name + " isn't on this Mac");

    if (info.loaded)
    {
        menu.addItem (showWindow, "Show Its Window");
        menu.addItem (bypass, "Bypass", true, info.bypassed);
        menu.addSeparator();
    }

    menu.addSubMenu (info.name.isEmpty() ? "Load an Effect" : "Replace With", load);

    if (info.name.isNotEmpty())
        menu.addItem (removeIt, "Remove");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (getSlotButton (bus, slot)),
                        [safeThis = SafePointer<RoutingScreen> (this), bus, slot, effects] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            auto& effectRack = safeThis->synthProcessor.getEffectRack();

                            if (result == showWindow)
                            {
                                safeThis->showPluginWindow (bus, slot);
                            }
                            else if (result == bypass)
                            {
                                effectRack.setBypassed (bus, slot, ! effectRack.getSlot (bus, slot).bypassed);
                            }
                            else if (result == removeIt)
                            {
                                effectRack.remove (bus, slot);
                            }
                            else if (juce::isPositiveAndBelow (result - 1000, effects.size()))
                            {
                                const auto error = effectRack.load (bus, slot, effects.getReference (result - 1000));

                                if (error.isNotEmpty())
                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "The effect couldn't be loaded", error);
                                else
                                    safeThis->showPluginWindow (bus, slot);
                            }

                            safeThis->showSlots();
                            safeThis->repaint();
                        });
}

void RoutingScreen::showPluginWindow (int bus, int slot)
{
    auto* plugin = synthProcessor.getEffectRack().getPlugin (bus, slot);

    if (plugin == nullptr)
        return;

    for (auto& window : pluginWindows)
    {
        if (&window->plugin == plugin)
        {
            window->toFront (true);
            return;
        }
    }

    const auto title = plugin->getName() + " (Bus " + juce::String (bus + 1) + ")";
    pluginWindows.push_back (std::make_unique<PluginWindow> (*plugin, title, [this] (PluginWindow& closed)
    {
        // Closed after the click that closed it has finished with it.
        juce::MessageManager::callAsync ([safeThis = SafePointer<RoutingScreen> (this), &shown = closed.plugin]
        {
            if (safeThis != nullptr)
                safeThis->closePluginWindow (shown);
        });
    }));
}

bool RoutingScreen::isPluginWindowShown (int bus, int slot) const
{
    const auto* plugin = synthProcessor.getEffectRack().getPlugin (bus, slot);
    return plugin != nullptr && std::any_of (pluginWindows.begin(), pluginWindows.end(),
                                             [plugin] (const auto& window) { return &window->plugin == plugin; });
}

void RoutingScreen::closePluginWindow (juce::AudioPluginInstance& plugin)
{
    pluginWindows.erase (std::remove_if (pluginWindows.begin(), pluginWindows.end(),
                                         [&plugin] (const auto& window) { return &window->plugin == &plugin; }),
                         pluginWindows.end());
}

void RoutingScreen::findPlugins()
{
    if (findPluginsWindow != nullptr)
    {
        findPluginsWindow->toFront (true);
        return;
    }

    auto& rack = synthProcessor.getEffectRack();
    auto list = std::make_unique<juce::PluginListComponent> (rack.getFormats(), rack.getKnownPlugins(), rack.getCrashedPluginFile(), nullptr, true);
    list->setSize (760, 520);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (list.release());
    options.dialogTitle = "Effect Plugins";
    options.dialogBackgroundColour = colours::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    findPluginsWindow = options.launchAsync();
}

juce::String RoutingScreen::describePath() const
{
    juce::String path = "Osc " + juce::String (oscillator + 1);

    for (int filter = 0; filter < modulation::numFilters; ++filter)
        if (synthProcessor.parameters.getRawParameterValue (SynthProcessor::filterSendId (oscillator, filter))->load() >= 0.5f)
            path += arrow + "Filter " + juce::String (filter + 1);

    if (! synthProcessor.hostsEffects())
        return path + arrow + "Out";

    juce::StringArray buses;
    const auto& rack = synthProcessor.getEffectRack();

    for (int bus = 0; bus < EffectRack::numBuses; ++bus)
    {
        if (synthProcessor.parameters.getRawParameterValue (SynthProcessor::busSendId (oscillator, bus))->load() < 0.5f)
            continue;

        juce::StringArray chain;

        for (int slot = 0; slot < EffectRack::slotsPerBus; ++slot)
            if (const auto info = rack.getSlot (bus, slot); info.loaded && ! info.bypassed)
                chain.add (info.name);

        buses.add ("Bus " + juce::String (bus + 1) + (chain.isEmpty() ? juce::String() : " (" + chain.joinIntoString (", ") + ")"));
    }

    if (buses.size() == 1)
        return path + arrow + buses[0] + arrow + "Out";

    if (buses.size() > 1)
        return path + arrow + buses.joinIntoString (" and ") + ", side by side" + arrow + "Out";

    return path + arrow + "Out";
}

void RoutingScreen::resized()
{
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (760, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    auto header = area.removeFromTop (30);
    doneButton.setBounds (header.removeFromRight (90));
    titleArea = header;

    if (pageButtons.isVisible())
        pageButtons.setBounds (header.removeFromRight (180).translated (-20, 0).withSizeKeepingCentre (180, 26));
    subtitleArea = area.removeFromTop (40);
    area.removeFromTop (12);

    auto chooser = area.removeFromTop (30);
    oscillatorCaptionArea = chooser.removeFromLeft (110);
    oscillatorButtons.setBounds (chooser.withWidth (juce::jmin (chooser.getWidth(), 64 * SynthProcessor::numOscillators)));
    area.removeFromTop (20);

    filtersCaptionArea = area.removeFromTop (18);
    area.removeFromTop (4);
    chainArea = area.removeFromTop (22);
    area.removeFromTop (8);

    const auto rowsTop = area;

    for (auto& row : rows)
    {
        row.area = area.removeFromTop (48);
        auto inner = row.area.reduced (14, 0);
        row.sendSwitch.setBounds (inner.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        area.removeFromTop (6);
    }

    // On the effects page, the button finding plugins beside the caption, and a row for each bus, with its switch, its
    // name and its slots, in order.
    findPluginsButton.setBounds (filtersCaptionArea.withLeft (filtersCaptionArea.getRight() - 130).withSizeKeepingCentre (130, 24));
    area = rowsTop;

    for (auto& row : busRows)
    {
        row.area = area.removeFromTop (48);
        auto inner = row.area.reduced (14, 0);
        row.sendSwitch.setBounds (inner.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        inner.removeFromLeft (16);
        row.titleArea = inner.removeFromLeft (60);
        inner.removeFromLeft (6);
        const auto slotWidth = (inner.getWidth() - 6 * (EffectRack::slotsPerBus - 1)) / EffectRack::slotsPerBus;

        for (auto& button : row.slots)
        {
            button.setBounds (inner.removeFromLeft (slotWidth).withSizeKeepingCentre (slotWidth, 28));
            inner.removeFromLeft (6);
        }

        area.removeFromTop (6);
    }
}

void RoutingScreen::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.96f));

    g.setColour (colours::panel);
    g.fillRoundedRectangle (panelArea.toFloat(), 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

    const auto name = "Osc " + juce::String (oscillator + 1);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("ROUTING", titleArea, juce::Justification::centredLeft);
    g.drawText ("OSCILLATOR", oscillatorCaptionArea, juce::Justification::centredLeft);

    const auto effects = page == Page::effects;
    g.drawText (effects ? "EFFECT BUSES" : "FILTERS", filtersCaptionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.drawFittedText (effects ? "Each bus plays the oscillators sent through it through its effects, from left to right. An oscillator "
                                "sent through no bus plays straight out; one sent through two plays through both, side by side."
                              : "Where each oscillator's sound goes. The filters run in a chain, from the first to the last; an "
                                "oscillator that isn't sent through a filter skips it, going straight on to the next.",
                      subtitleArea, juce::Justification::centredLeft, 2);

    // The path the oscillator's sound takes, through the filters and buses it's sent through.
    g.setColour (colours::text);
    g.drawText (describePath(), chainArea, juce::Justification::centredLeft, true);

    if (effects)
    {
        for (size_t bus = 0; bus < busRows.size(); ++bus)
        {
            const auto& row = busRows[bus];
            const auto sent = row.sendSwitch.getToggleState();

            g.setColour (colours::background.withAlpha (sent ? 0.5f : 0.25f));
            g.fillRoundedRectangle (row.area.toFloat(), 6.0f);
            g.setFont (fonts::body (14.0f).boldened());
            g.setColour (sent ? colours::text : colours::dimText);
            g.drawText ("Bus " + juce::String ((int) bus + 1), row.titleArea, juce::Justification::centredLeft);
        }

        return;
    }

    for (size_t filter = 0; filter < rows.size(); ++filter)
    {
        const auto& row = rows[filter];
        const auto sent = row.sendSwitch.getToggleState();

        g.setColour (colours::background.withAlpha (sent ? 0.5f : 0.25f));
        g.fillRoundedRectangle (row.area.toFloat(), 6.0f);

        auto text = row.area.reduced (14, 0);
        text.removeFromLeft (36 + 16);
        auto status = text.removeFromRight (220);

        g.setFont (fonts::body (14.0f).boldened());
        g.setColour (sent ? colours::text : colours::dimText);
        const auto title = "Filter " + juce::String ((int) filter + 1);
        g.drawText (title, text.removeFromLeft (80), juce::Justification::centredLeft);

        g.setFont (fonts::body (13.0f));
        g.setColour (colours::dimText);
        g.drawText (row.shownSettings, text, juce::Justification::centredLeft, true);

        g.setColour (sent ? colours::accent : colours::dimText);
        g.drawText (sent ? name + " is sent through it" : name + " skips it", status, juce::Justification::centredRight);
    }
}

bool RoutingScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose != nullptr)
            onClose();

        return true;
    }

    return false;
}
