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

    const std::array<const char*, 4> ids { SynthProcessor::reverbDelayId, SynthProcessor::reverbSizeId,
                                           SynthProcessor::reverbDecayId, SynthProcessor::reverbMixId };
    const std::array<const char*, 4> captions { "PRE-DELAY", "SIZE", "DECAY", "MIX" };
    const std::array<const char*, 4> hints { "Before the first reflection", "Of the room, from a box to a hall",
                                             "For the tail to fall 60 dB", "Of the reverb, the rest dry" };

    for (size_t i = 0; i < dials.size(); ++i)
    {
        auto& dial = dials[i];
        const auto& parameter = *synthProcessor.parameters.getParameter (ids[i]);
        dial.caption = captions[i];
        dial.hint = hints[i];
        dial.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        dial.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dial.knob.setTitle ("Reverb " + dial.caption.toLowerCase());
        dial.knob.setMouseClickGrabsKeyboardFocus (false);
        dial.knob.setWantsKeyboardFocus (false);
        dial.knob.onValueChange = [this, &dial] { repaint (dial.valueArea); };
        addAndMakeVisible (dial.knob);
        dial.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (synthProcessor.parameters, ids[i], dial.knob);
        dial.knob.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
    }

    for (size_t i = 0; i < sends.size(); ++i)
    {
        auto& send = sends[i];
        send.sendSwitch.setTitle ("Osc " + juce::String ((int) i + 1) + " to reverb");
        send.sendSwitch.setMouseClickGrabsKeyboardFocus (false);
        send.sendSwitch.setWantsKeyboardFocus (false);
        send.sendSwitch.onStateChange = [this] { repaint(); };
        addAndMakeVisible (send.sendSwitch);
    }

    attachSends();
}

void EffectsScreen::attachSends()
{
    // The switches set the pitch set the controls set, as the routing screen's do.
    for (size_t i = 0; i < sends.size(); ++i)
    {
        auto& send = sends[i];
        send.attachment.reset();
        send.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            synthProcessor.parameters, SynthProcessor::effectSendId ((int) i, Patch::reverbEffect), send.sendSwitch);
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
    area.removeFromTop (16);

    // The reverb: its four dials side by side, each with its caption, value and what it sets, and under them which
    // oscillators are sent through it.
    reverbArea = area.removeFromTop (290);
    auto reverb = reverbArea.reduced (16, 14);
    reverbCaptionArea = reverb.removeFromTop (18);
    reverbNoteArea = reverbCaptionArea.withTrimmedLeft (90);
    reverb.removeFromTop (12);

    auto dialRow = reverb.removeFromTop (150);
    const auto dialWidth = dialRow.getWidth() / (int) dials.size();

    for (auto& dial : dials)
    {
        auto column = dialRow.removeFromLeft (dialWidth);
        dial.captionArea = column.removeFromTop (16);
        dial.hintArea = column.removeFromBottom (16);
        dial.valueArea = column.removeFromBottom (20);
        dial.knob.setBounds (column.withSizeKeepingCentre (juce::jmin (column.getWidth(), 96), column.getHeight()));
    }

    reverb.removeFromTop (14);
    sendsCaptionArea = reverb.removeFromTop (18);
    sendsNoteArea = sendsCaptionArea.withTrimmedLeft (160);
    reverb.removeFromTop (8);
    auto row = reverb.removeFromTop (40);
    const auto sendWidth = row.getWidth() / (int) sends.size();

    for (auto& send : sends)
    {
        auto cell = row.removeFromLeft (sendWidth).reduced (4, 0);
        send.sendSwitch.setBounds (cell.removeFromLeft (36).withSizeKeepingCentre (36, 22));
        cell.removeFromLeft (8);
        send.labelArea = cell;
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
    g.drawFittedText ("Effects the oscillators can be sent through, after their filters. Choose which here, or on each "
                      "oscillator's routing screen; one that isn't sent through an effect plays straight out.",
                      subtitleArea, juce::Justification::centredLeft, 2);

    g.setColour (colours::background.withAlpha (0.5f));
    g.fillRoundedRectangle (reverbArea.toFloat(), 8.0f);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("REVERB", reverbCaptionArea, juce::Justification::centredLeft);
    g.drawText ("SENT THROUGH IT", sendsCaptionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (12.5f));
    g.drawText ("The same for every pitch set", reverbNoteArea, juce::Justification::centredRight);

    if (synthProcessor.getNumPitchSets() > 1)
        g.drawText ("For " + synthProcessor.getPitchSetName (synthProcessor.getActivePitchSet()), sendsNoteArea,
                    juce::Justification::centredRight);

    for (auto& dial : dials)
    {
        g.setFont (fonts::caption());
        g.setColour (colours::dimText);
        g.drawText (dial.caption, dial.captionArea, juce::Justification::centred);

        g.setFont (fonts::body (14.0f));
        g.setColour (colours::text);
        g.drawText (dial.knob.getTextFromValue (dial.knob.getValue()), dial.valueArea, juce::Justification::centred);

        g.setFont (fonts::body (12.0f));
        g.setColour (colours::dimText);
        fonts::drawFirstThatFits (g, { dial.hint }, dial.hintArea, juce::Justification::centred);
    }

    for (size_t i = 0; i < sends.size(); ++i)
    {
        const auto sent = sends[i].sendSwitch.getToggleState();
        g.setFont (fonts::body (14.0f).boldened());
        g.setColour (sent ? colours::text : colours::dimText);
        g.drawText ("Osc " + juce::String ((int) i + 1), sends[i].labelArea, juce::Justification::centredLeft);
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
