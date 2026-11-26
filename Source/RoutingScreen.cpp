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

    std::vector<Row*> allRows;

    for (auto& row : rows)
        allRows.push_back (&row);

    for (size_t effect = 0; effect < effectRows.size(); ++effect)
    {
        auto& row = effectRows[effect];
        allRows.push_back (&row);
        row.settingsButton = std::make_unique<juce::TextButton> ("Settings");
        row.settingsButton->setTooltip ("Set the " + SynthProcessor::getEffectName ((int) effect).toLowerCase() + ", on the effects' screen");
        row.settingsButton->setMouseClickGrabsKeyboardFocus (false);
        row.settingsButton->setWantsKeyboardFocus (false);
        row.settingsButton->onClick = [this]
        {
            if (onShowEffects != nullptr)
                onShowEffects();
        };
        addAndMakeVisible (*row.settingsButton);
    }

    for (auto* each : allRows)
    {
        auto& row = *each;
        row.sendSwitch.setMouseClickGrabsKeyboardFocus (false);
        row.sendSwitch.setWantsKeyboardFocus (false);
        row.sendSwitch.onStateChange = [this] { repaint(); };
        addAndMakeVisible (row.sendSwitch);
    }

    showOscillator (0);
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

    for (size_t effect = 0; effect < effectRows.size(); ++effect)
    {
        auto& row = effectRows[effect];
        row.attachment.reset();
        row.sendSwitch.setTitle ("Osc " + juce::String (oscillator + 1) + " to " + SynthProcessor::getEffectName ((int) effect).toLowerCase());
        row.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            synthProcessor.parameters, SynthProcessor::effectSendId (oscillator, (int) effect), row.sendSwitch);
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

juce::String RoutingScreen::describeEffect (int effect) const
{
    const auto text = [this] (const char* id) { return synthProcessor.parameters.getParameter (id)->getCurrentValueAsText(); };

    if (synthProcessor.parameters.getRawParameterValue (SynthProcessor::effectOnId (effect))->load() < 0.5f)
        return "switched off, so it plays dry";

    if (effect == Patch::chorusEffect)
        return text (SynthProcessor::chorusRateId) + dot + text (SynthProcessor::chorusDepthId) + " deep" + dot
             + text (SynthProcessor::chorusDelayId) + dot + text (SynthProcessor::chorusMixId) + " wet";

    if (effect == Patch::delayEffect)
    {
        const auto synced = synthProcessor.parameters.getRawParameterValue (SynthProcessor::delaySyncId)->load() >= 0.5f;
        return text (synced ? SynthProcessor::delayDivisionId : SynthProcessor::delayTimeId) + dot
             + text (SynthProcessor::delayFeedbackId) + " feedback" + dot + text (SynthProcessor::delayMixId) + " wet";
    }

    return text (SynthProcessor::reverbDelayId) + dot + "size " + text (SynthProcessor::reverbSizeId) + dot
         + text (SynthProcessor::reverbDecayId) + dot + text (SynthProcessor::reverbMixId) + " wet";
}

juce::String RoutingScreen::describePath() const
{
    // The path the oscillator's sound takes, through the filters and effects it's sent through.
    juce::String path = "Osc " + juce::String (oscillator + 1);

    for (size_t filter = 0; filter < rows.size(); ++filter)
        if (rows[filter].sendSwitch.getToggleState())
            path += arrow + "Filter " + juce::String ((int) filter + 1);

    for (size_t effect = 0; effect < effectRows.size(); ++effect)
        if (effectRows[effect].sendSwitch.getToggleState())
            path += arrow + SynthProcessor::getEffectName ((int) effect);

    return path + arrow + "Out";
}

void RoutingScreen::refresh()
{
    auto changed = false;

    for (size_t effect = 0; effect < effectRows.size(); ++effect)
    {
        const auto settings = describeEffect ((int) effect);
        changed = changed || settings != effectRows[effect].shownSettings;
        effectRows[effect].shownSettings = settings;
    }

    for (size_t filter = 0; filter < rows.size(); ++filter)
    {
        const auto settings = describeFilter ((int) filter);
        changed = changed || settings != rows[filter].shownSettings;
        rows[filter].shownSettings = settings;
    }

    if (changed)
        repaint();
}

void RoutingScreen::resized()
{
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (760, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    auto header = area.removeFromTop (30);
    doneButton.setBounds (header.removeFromRight (90));
    titleArea = header;
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

    for (auto& row : rows)
    {
        row.area = area.removeFromTop (48);
        auto inner = row.area.reduced (14, 0);
        row.sendSwitch.setBounds (inner.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        area.removeFromTop (6);
    }

    area.removeFromTop (14);
    effectsCaptionArea = area.removeFromTop (18);
    area.removeFromTop (8);

    for (auto& row : effectRows)
    {
        row.area = area.removeFromTop (48);
        auto inner = row.area.reduced (14, 0);
        row.sendSwitch.setBounds (inner.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        row.settingsButton->setBounds (inner.removeFromRight (90).withSizeKeepingCentre (90, 26));
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
    g.drawText ("FILTERS", filtersCaptionArea, juce::Justification::centredLeft);
    g.drawText ("EFFECTS", effectsCaptionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.drawFittedText ("Where each oscillator's sound goes. The filters run in a chain, from the first to the last; an "
                      "oscillator that isn't sent through a filter or effect skips it, going straight on to the next. After "
                      "the filters come the effects, the chorus, the delay, then the reverb.",
                      subtitleArea, juce::Justification::centredLeft, 2);

    g.setColour (colours::text);
    g.drawText (describePath(), chainArea, juce::Justification::centredLeft);

    for (size_t filter = 0; filter < rows.size(); ++filter)
        paintRow (g, rows[filter], "Filter " + juce::String ((int) filter + 1), name + " is sent through it", name + " skips it");

    for (size_t effect = 0; effect < effectRows.size(); ++effect)
        paintRow (g, effectRows[effect], SynthProcessor::getEffectName ((int) effect), name + " is sent through it",
                  name + " skips it");
}

void RoutingScreen::paintRow (juce::Graphics& g, const Row& row, const juce::String& title, const juce::String& sentText,
                              const juce::String& skipText) const
{
    const auto sent = row.sendSwitch.getToggleState();

    g.setColour (colours::background.withAlpha (sent ? 0.5f : 0.25f));
    g.fillRoundedRectangle (row.area.toFloat(), 6.0f);

    auto text = row.area.reduced (14, 0);
    text.removeFromLeft (36 + 16);

    // An effect's row has its Settings button at the end.
    if (row.settingsButton != nullptr)
        text.removeFromRight (90 + 16);

    auto status = text.removeFromRight (200);

    g.setFont (fonts::body (14.0f).boldened());
    g.setColour (sent ? colours::text : colours::dimText);
    g.drawText (title, text.removeFromLeft (80), juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::dimText);
    g.drawText (row.shownSettings, text, juce::Justification::centredLeft, true);

    g.setColour (sent ? colours::accent : colours::dimText);
    g.drawText (sent ? sentText : skipText, status, juce::Justification::centredRight);
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
