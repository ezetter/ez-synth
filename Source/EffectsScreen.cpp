#include "EffectsScreen.h"

#include "SynthLookAndFeel.h"

EffectsScreen::EffectsScreen (SynthProcessor& processor)
    : synthProcessor (processor)
{
    setWantsKeyboardFocus (true);

    doneButton.setMouseClickGrabsKeyboardFocus (false);
    doneButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (doneButton);

    for (size_t effect = 0; effect < sections.size(); ++effect)
        sections[effect].effect = (int) effect;

    auto& delay = sections[(size_t) Patch::delayEffect];
    addDial (delay, SynthProcessor::delayTimeId, "TIME", "Before each repeat");
    addDial (delay, SynthProcessor::delayDivisionId, "TIME", "A note at the tempo");
    addDial (delay, SynthProcessor::delayFeedbackId, "FEEDBACK", "Of each repeat, again");
    addDial (delay, SynthProcessor::delayMixId, "MIX", "Of the repeats, the rest dry");

    auto& reverb = sections[(size_t) Patch::reverbEffect];
    addDial (reverb, SynthProcessor::reverbDelayId, "PRE-DELAY", "Before the first reflection");
    addDial (reverb, SynthProcessor::reverbSizeId, "SIZE", "Of the room, from a box to a hall");
    addDial (reverb, SynthProcessor::reverbDecayId, "DECAY", "For the tail to fall 60 dB");
    addDial (reverb, SynthProcessor::reverbMixId, "MIX", "Of the reverb, the rest dry");

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
}

EffectsScreen::Dial& EffectsScreen::addDial (Section& section, const char* parameterId, const juce::String& caption,
                                             const juce::String& hint)
{
    auto& dial = allDials.emplace_back();
    const auto& parameter = *synthProcessor.parameters.getParameter (parameterId);
    dial.caption = caption;
    dial.hint = hint;
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
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (760, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    auto header = area.removeFromTop (30);
    doneButton.setBounds (header.removeFromRight (90));
    titleArea = header;
    subtitleArea = area.removeFromTop (40);
    area.removeFromTop (12);

    // Each effect in turn, as they run: its dials side by side, each with its caption, value and what it sets, and
    // under them which oscillators are sent through it. They share the height, up to as much as they need.
    const auto sectionHeight = juce::jmin (270, (area.getHeight() - 12 * ((int) sections.size() - 1)) / (int) sections.size());

    for (auto& section : sections)
    {
        section.area = area.removeFromTop (sectionHeight);
        area.removeFromTop (12);
        auto inside = section.area.reduced (16, 12);

        section.captionArea = inside.removeFromTop (24);

        if (section.effect == Patch::delayEffect)
            delaySync.setBounds (section.captionArea.withTrimmedLeft (90).withWidth (220));

        inside.removeFromTop (8);

        auto sends = inside.removeFromBottom (34);
        inside.removeFromBottom (6);
        section.sendsCaptionArea = inside.removeFromBottom (18);
        section.sendsNoteArea = section.sendsCaptionArea.withTrimmedLeft (160);
        inside.removeFromBottom (8);

        const auto dials = getShownDials (section);
        const auto dialWidth = inside.getWidth() / 4;

        for (auto* dial : dials)
        {
            auto column = inside.removeFromLeft (dialWidth);
            dial->captionArea = column.removeFromTop (16);
            dial->hintArea = column.removeFromBottom (16);
            dial->valueArea = column.removeFromBottom (20);
            dial->knob.setBounds (column.withSizeKeepingCentre (juce::jmin (column.getWidth(), 96), column.getHeight()));
        }

        const auto sendWidth = sends.getWidth() / (int) section.sends.size();

        for (auto& send : section.sends)
        {
            auto cell = sends.removeFromLeft (sendWidth).reduced (4, 0);
            send.sendSwitch.setBounds (cell.removeFromLeft (36).withSizeKeepingCentre (36, 22));
            cell.removeFromLeft (8);
            send.labelArea = cell;
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
    g.drawFittedText ("Effects the oscillators can be sent through, after their filters, in a chain: the delay, then the reverb. "
                      "Each is set the same for every pitch set. Choose which oscillators go through each here, or on each "
                      "oscillator's routing screen; one sent through none plays straight out.",
                      subtitleArea, juce::Justification::centredLeft, 2);

    const auto several = synthProcessor.getNumPitchSets() > 1;

    for (auto& section : sections)
    {
        g.setColour (colours::background.withAlpha (0.5f));
        g.fillRoundedRectangle (section.area.toFloat(), 8.0f);

        g.setFont (fonts::caption());
        g.setColour (colours::dimText);
        g.drawText (SynthProcessor::getEffectName (section.effect).toUpperCase(), section.captionArea, juce::Justification::centredLeft);
        g.drawText ("SENT THROUGH IT", section.sendsCaptionArea, juce::Justification::centredLeft);

        if (several)
        {
            g.setFont (fonts::body (12.5f));
            g.drawText ("For " + synthProcessor.getPitchSetName (synthProcessor.getActivePitchSet()), section.sendsNoteArea,
                        juce::Justification::centredRight);
        }

        for (auto* dial : getShownDials (section))
        {
            g.setFont (fonts::caption());
            g.setColour (colours::dimText);
            g.drawText (dial->caption, dial->captionArea, juce::Justification::centred);

            g.setFont (fonts::body (14.0f));
            g.setColour (colours::text);
            g.drawText (dial->knob.getTextFromValue (dial->knob.getValue()), dial->valueArea, juce::Justification::centred);

            g.setFont (fonts::body (12.0f));
            g.setColour (colours::dimText);
            fonts::drawFirstThatFits (g, { dial->hint }, dial->hintArea, juce::Justification::centred);
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
