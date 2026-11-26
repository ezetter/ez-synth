#include "EffectsScreen.h"

#include "SynthLookAndFeel.h"

EffectsScreen::EffectsScreen (SynthProcessor& processor)
    : synthProcessor (processor)
{
    setWantsKeyboardFocus (true);

    for (size_t effect = 0; effect < sections.size(); ++effect)
        sections[effect].effect = (int) effect;

    auto& chorus = sections[(size_t) Patch::chorusEffect];
    addDial (chorus, SynthProcessor::chorusRateId, "RATE", { "Of the sweep, up and down", "Of the sweep" });
    addDial (chorus, SynthProcessor::chorusDepthId, "DEPTH", { "How far the time swings", "How far it swings" });
    addDial (chorus, SynthProcessor::chorusDelayId, "DELAY", { "The time it swings around", "It swings around" });
    addDial (chorus, SynthProcessor::chorusFeedbackId, "FEEDBACK", { "Of the copy, again" });
    addDial (chorus, SynthProcessor::chorusMixId, "MIX", { "Of the copy, the rest dry", "Of the copy" });

    auto& delay = sections[(size_t) Patch::delayEffect];
    addDial (delay, SynthProcessor::delayTimeId, "TIME", { "Before each repeat" });
    addDial (delay, SynthProcessor::delayDivisionId, "TIME", { "A note at the tempo" });
    addDial (delay, SynthProcessor::delayFeedbackId, "FEEDBACK", { "Of each repeat, again", "Of each repeat" });
    addDial (delay, SynthProcessor::delayMixId, "MIX", { "Of the repeats, the rest dry", "Of the repeats" });

    auto& reverb = sections[(size_t) Patch::reverbEffect];
    addDial (reverb, SynthProcessor::reverbDelayId, "PRE-DELAY", { "Before the first reflection", "Before reflections" });
    addDial (reverb, SynthProcessor::reverbSizeId, "SIZE", { "Of the room, from a box to a hall", "Of the room" });
    addDial (reverb, SynthProcessor::reverbDecayId, "DECAY", { "For the tail to fall 60 dB", "To fall 60 dB" });
    addDial (reverb, SynthProcessor::reverbMixId, "MIX", { "Of the reverb, the rest dry", "Of the reverb" });

    delaySync.setTitle ("Delay time");
    delaySync.onChoose = [this] (int chosen) { delaySyncAttachment->setValueAsCompleteGesture ((float) chosen); };
    addAndMakeVisible (delaySync);
    delaySyncAttachment = std::make_unique<juce::ParameterAttachment> (*synthProcessor.parameters.getParameter (SynthProcessor::delaySyncId),
                                                                       [this] (float value)
                                                                       {
                                                                           delaySync.setChosen (value >= 0.5f ? 1 : 0);
                                                                           showDelaySync();
                                                                       });

    for (auto& section : sections)
    {
        const auto name = SynthProcessor::getEffectName (section.effect).toLowerCase();

        section.onSwitch.setTitle (SynthProcessor::getEffectName (section.effect) + " on");
        section.onSwitch.setTooltip ("Switch the " + name + " on; off, what's sent through it plays dry");
        section.onSwitch.setMouseClickGrabsKeyboardFocus (false);
        section.onSwitch.setWantsKeyboardFocus (false);
        section.onSwitch.onStateChange = [this] { showOnOff(); };
        addAndMakeVisible (section.onSwitch);
        section.onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            synthProcessor.parameters, SynthProcessor::effectOnId (section.effect), section.onSwitch);

        for (size_t i = 0; i < section.sends.size(); ++i)
        {
            auto& send = section.sends[i];
            send.sendSwitch.setTitle ("Osc " + juce::String ((int) i + 1) + " to " + name);
            send.sendSwitch.setMouseClickGrabsKeyboardFocus (false);
            send.sendSwitch.setWantsKeyboardFocus (false);
            send.sendSwitch.onStateChange = [this] { repaint(); };
            addAndMakeVisible (send.sendSwitch);
        }
    }

    attachSends();
    delaySyncAttachment->sendInitialUpdate();
    showOnOff();
}

void EffectsScreen::showOnOff()
{
    for (auto& section : sections)
    {
        const auto on = section.onSwitch.getToggleState();
        const auto colour = on ? colours::accent : colours::dimText.withAlpha (0.6f);

        for (auto* dial : section.dials)
            dial->knob.setColour (juce::Slider::rotarySliderFillColourId, colour);

        if (section.effect == Patch::delayEffect)
            delaySync.setAlpha (on ? 1.0f : 0.55f);
    }

    repaint();
}

EffectsScreen::Dial& EffectsScreen::addDial (Section& section, const char* parameterId, const juce::String& caption,
                                             const juce::StringArray& hints)
{
    auto& dial = allDials.emplace_back();
    const auto& parameter = *synthProcessor.parameters.getParameter (parameterId);
    dial.caption = caption;
    dial.hints = hints;
    dial.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    dial.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    dial.knob.setTitle (SynthProcessor::getEffectName (section.effect) + " " + caption.toLowerCase());
    dial.knob.setMouseClickGrabsKeyboardFocus (false);
    dial.knob.setWantsKeyboardFocus (false);
    dial.knob.onValueChange = [this, &dial] { repaint (dial.valueArea); };
    addAndMakeVisible (dial.knob);
    dial.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (synthProcessor.parameters, parameterId, dial.knob);
    dial.knob.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
    section.dials.push_back (&dial);
    return dial;
}

std::vector<EffectsScreen::Dial*> EffectsScreen::getShownDials (const Section& section) const
{
    // The delay shows its time in milliseconds or as a note, as it's synced or not.
    if (section.effect != Patch::delayEffect)
        return section.dials;

    const auto synced = delaySync.getChosen() == 1;
    return { section.dials[synced ? 1 : 0], section.dials[2], section.dials[3] };
}

void EffectsScreen::showDelaySync()
{
    const auto& delay = sections[(size_t) Patch::delayEffect];
    const auto shown = getShownDials (delay);

    for (auto* dial : delay.dials)
        dial->knob.setVisible (std::find (shown.begin(), shown.end(), dial) != shown.end());

    resized();
    repaint();
}

void EffectsScreen::attachSends()
{
    // The switches set the pitch set the controls set, as the routing screen's do.
    for (auto& section : sections)
    {
        for (size_t i = 0; i < section.sends.size(); ++i)
        {
            auto& send = section.sends[i];
            send.attachment.reset();
            send.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                synthProcessor.parameters, SynthProcessor::effectSendId ((int) i, section.effect), send.sendSwitch);
        }
    }

    attachedPitchSetVersion = synthProcessor.getPitchSetVersion();
}

void EffectsScreen::refresh()
{
    if (attachedPitchSetVersion != synthProcessor.getPitchSetVersion())
    {
        attachSends();
        repaint();
    }
}

void EffectsScreen::resized()
{
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (1240, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    titleArea = area.removeFromTop (30);
    subtitleArea = area.removeFromTop (40);
    area.removeFromTop (12);

    // A row for each effect, as they run: its switch and name, then its dials side by side, each with its caption,
    // value and what it sets, in columns that line up from one to the next, and last which oscillators are sent
    // through it. They share the height, up to as much as they need.
    const auto gap = 10;
    const auto sectionHeight = juce::jmin (200, (area.getHeight() - gap * ((int) sections.size() - 1)) / (int) sections.size());
    constexpr auto nameWidth = 190, sendsWidth = 300, mostDials = 5;

    for (auto& section : sections)
    {
        section.area = area.removeFromTop (sectionHeight);
        area.removeFromTop (gap);
        auto inside = section.area.reduced (16, 12);

        auto left = inside.removeFromLeft (nameWidth);
        inside.removeFromLeft (16);
        section.captionArea = left.removeFromTop (24);
        auto caption = section.captionArea;
        section.onSwitch.setBounds (caption.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        caption.removeFromLeft (10);
        section.nameArea = caption;
        left.removeFromTop (10);

        if (section.effect == Patch::delayEffect)
        {
            delaySync.setBounds (left.removeFromTop (28));
            left.removeFromTop (8);
        }

        section.offArea = left.withHeight (juce::jmin (left.getHeight(), 34));

        // The oscillators' switches, three to a row.
        auto sends = inside.removeFromRight (sendsWidth);
        inside.removeFromRight (16);
        constexpr auto perRow = 3, rowHeight = 28;
        const auto rows = ((int) section.sends.size() + perRow - 1) / perRow;
        sends = sends.withSizeKeepingCentre (sends.getWidth(), juce::jmin (sends.getHeight(), 18 + 6 + rows * rowHeight));
        section.sendsCaptionArea = sends.removeFromTop (18);
        section.sendsNoteArea = section.sendsCaptionArea.withTrimmedLeft (130);
        sends.removeFromTop (6);

        for (size_t i = 0; i < section.sends.size(); ++i)
        {
            auto& send = section.sends[i];
            auto cell = juce::Rectangle<int> (sends.getX() + ((int) i % perRow) * sends.getWidth() / perRow,
                                              sends.getY() + ((int) i / perRow) * rowHeight,
                                              sends.getWidth() / perRow, rowHeight);
            send.sendSwitch.setBounds (cell.removeFromLeft (36).withSizeKeepingCentre (36, 22));
            cell.removeFromLeft (8);
            send.labelArea = cell;
        }

        const auto dialWidth = inside.getWidth() / mostDials;

        for (auto* dial : getShownDials (section))
        {
            auto column = inside.removeFromLeft (dialWidth);
            dial->captionArea = column.removeFromTop (16);
            dial->hintArea = column.removeFromBottom (16);
            dial->valueArea = column.removeFromBottom (20);
            dial->knob.setBounds (column.withSizeKeepingCentre (juce::jmin (column.getWidth(), 96), column.getHeight()));
        }
    }
}

void EffectsScreen::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.96f));

    g.setColour (colours::panel);
    g.fillRoundedRectangle (panelArea.toFloat(), 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("EFFECTS", titleArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.drawFittedText ("Effects the oscillators can be sent through, after their filters, in a chain: the chorus, then the delay, "
                      "then the reverb. Each is set the same for every pitch set. Choose which oscillators go through each here, "
                      "or on each oscillator's routing screen; one sent through none plays straight out.",
                      subtitleArea, juce::Justification::centredLeft, 2, 1.0f);

    const auto several = synthProcessor.getNumPitchSets() > 1;

    for (auto& section : sections)
    {
        g.setColour (colours::background.withAlpha (0.5f));
        g.fillRoundedRectangle (section.area.toFloat(), 8.0f);

        const auto on = section.onSwitch.getToggleState();
        g.setFont (fonts::caption());
        g.setColour (on ? colours::text : colours::dimText);
        g.drawText (SynthProcessor::getEffectName (section.effect).toUpperCase(), section.nameArea, juce::Justification::centredLeft);

        if (! on)
        {
            g.setFont (fonts::body (12.5f));
            g.setColour (colours::dimText);
            g.drawFittedText ("Off, so what's sent through it plays dry", section.offArea, juce::Justification::topLeft, 2, 1.0f);
        }

        g.setFont (fonts::caption());
        g.setColour (colours::dimText);
        g.drawText ("SENT THROUGH IT", section.sendsCaptionArea, juce::Justification::centredLeft);

        if (several)
        {
            g.setFont (fonts::body (12.5f));
            fonts::drawFirstThatFits (g, { "For " + synthProcessor.getPitchSetName (synthProcessor.getActivePitchSet()) },
                                      section.sendsNoteArea, juce::Justification::centredRight);
        }

        for (auto* dial : getShownDials (section))
        {
            g.setFont (fonts::caption());
            g.setColour (colours::dimText);
            g.drawText (dial->caption, dial->captionArea, juce::Justification::centred);

            g.setFont (fonts::body (14.0f));
            g.setColour (on ? colours::text : colours::dimText);
            g.drawText (dial->knob.getTextFromValue (dial->knob.getValue()), dial->valueArea, juce::Justification::centred);

            g.setFont (fonts::body (12.0f));
            g.setColour (colours::dimText);
            fonts::drawFirstThatFits (g, dial->hints, dial->hintArea, juce::Justification::centred);
        }

        for (size_t i = 0; i < section.sends.size(); ++i)
        {
            const auto sent = section.sends[i].sendSwitch.getToggleState();
            g.setFont (fonts::body (14.0f).boldened());
            g.setColour (sent ? colours::text : colours::dimText);
            g.drawText ("Osc " + juce::String ((int) i + 1), section.sends[i].labelArea, juce::Justification::centredLeft);
        }
    }
}

bool EffectsScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose != nullptr)
            onClose();

        return true;
    }

    return false;
}
