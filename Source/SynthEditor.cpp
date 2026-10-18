#include "SynthEditor.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));
}

SynthEditor::SynthEditor (SynthProcessor& p)
    : AudioProcessorEditor (p),
      synthProcessor (p),
      harmonicSelector (*p.parameters.getParameter (SynthProcessor::harmonicsId)),
      volumeAttachment (p.parameters, SynthProcessor::volumeId, volumeKnob),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    grid.onDraw = [this] (const waveform::Shape& shape)
    {
        synthProcessor.setShape (shape);
        refresh();
    };

    addAndMakeVisible (grid);
    addAndMakeVisible (legend);
    addAndMakeVisible (harmonicSelector);

    for (size_t i = 0; i < presetButtons.size(); ++i)
    {
        const auto preset = waveform::allPresets[i];
        auto& button = presetButtons[i];
        button.setButtonText (waveform::getName (preset));
        button.setMouseClickGrabsKeyboardFocus (false);
        button.setWantsKeyboardFocus (false);
        button.onClick = [this, preset]
        {
            synthProcessor.setShape (waveform::makePreset (preset), preset);
            refresh();
        };
        addAndMakeVisible (button);
    }

    const auto& volumeParameter = *p.parameters.getParameter (SynthProcessor::volumeId);
    volumeKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volumeKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeKnob.setDoubleClickReturnValue (true, volumeParameter.convertFrom0to1 (volumeParameter.getDefaultValue()));
    volumeKnob.setMouseClickGrabsKeyboardFocus (false);
    volumeKnob.setWantsKeyboardFocus (false);
    volumeKnob.setTitle ("Volume");
    volumeKnob.onValueChange = [this] { repaint (volumeValue); };
    addAndMakeVisible (volumeKnob);

    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (960, 640, 1800, 1200);
    setSize (1100, 740);

    refresh();
    startTimerHz (30);
}

SynthEditor::~SynthEditor()
{
    setLookAndFeel (nullptr);
}

void SynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto title = titleArea.toFloat();
    g.setColour (colours::text);
    g.setFont (fonts::title());
    g.drawText ("EZ SYNTH", title.removeFromTop (28.0f), juce::Justification::bottomLeft);
    g.setColour (colours::dimText);
    g.setFont (fonts::body (13.0f));
    g.drawText ("Draw a wave. Hear its harmonics.", title, juce::Justification::topLeft);

    for (const auto& panel : { sidePanel, volumePanel })
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (panel.toFloat(), 10.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("HARMONICS", harmonicsCaption, juce::Justification::centredLeft);
    g.drawText ("VOLUME", volumeCaption, juce::Justification::centred);

    const auto numPartials = synthProcessor.getNumPartials();
    g.setFont (fonts::body (12.5f));
    g.drawFittedText ("The wave is rebuilt from its "
                          + (numPartials == 1 ? juce::String ("strongest sine wave.")
                                              : juce::String (numPartials) + " strongest sine waves."),
                      harmonicsHint, juce::Justification::topLeft, 2);

    g.setColour (colours::text);
    g.setFont (fonts::body (14.0f));
    g.drawText (juce::String (juce::roundToInt (volumeKnob.getValue() * 100.0)) + "%", volumeValue,
                juce::Justification::centred);

    paintNote (g);
}

void SynthEditor::paintNote (juce::Graphics& g) const
{
    auto area = noteArea.toFloat();
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("FUNDAMENTAL", area.removeFromTop (20.0f), juce::Justification::bottomRight);

    const auto note = synthProcessor.getLastNote();

    if (note < 0)
    {
        g.setFont (fonts::body (16.0f));
        g.drawText ("Play a key", area, juce::Justification::centredRight);
        return;
    }

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4) + "  " + dot + "  "
                    + juce::String (juce::MidiMessage::getMidiNoteInHertz (note), 2) + " Hz",
                area, juce::Justification::centredRight);
}

void SynthEditor::resized()
{
    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (50);
    titleArea = header.removeFromLeft (250);
    noteArea = header.removeFromRight (240);

    auto presets = header.withSizeKeepingCentre (header.getWidth(), 34);

    for (auto& button : presetButtons)
    {
        button.setBounds (presets.removeFromLeft (96));
        presets.removeFromLeft (8);
    }

    area.removeFromTop (14);

    auto bottom = area.removeFromBottom (juce::jlimit (110, 170, getHeight() / 5));
    volumePanel = bottom.removeFromLeft (120);
    bottom.removeFromLeft (12);
    keyboard.setBounds (bottom);

    auto volume = volumePanel.reduced (10, 8);
    volumeCaption = volume.removeFromTop (16);
    volumeValue = volume.removeFromBottom (20);
    volumeKnob.setBounds (volume);

    area.removeFromBottom (14);

    sidePanel = area.removeFromRight (290);
    area.removeFromRight (14);
    grid.setBounds (area);

    auto side = sidePanel.reduced (16, 14);
    harmonicsCaption = side.removeFromTop (18);
    side.removeFromTop (8);
    harmonicSelector.setBounds (side.removeFromTop (34));
    side.removeFromTop (10);
    harmonicsHint = side.removeFromTop (36);
    side.removeFromTop (8);
    legend.setBounds (side);
}

void SynthEditor::refresh()
{
    const auto shapeVersion = synthProcessor.getShapeVersion();
    const auto numPartials = synthProcessor.getNumPartials();

    if (shapeVersion != shownShapeVersion || numPartials != shownNumPartials)
    {
        const auto& decomposition = synthProcessor.getDecomposition();
        const auto numUsed = decomposition.getNumUsed (numPartials);
        std::vector<waveform::Partial> partials (decomposition.strongest.begin(),
                                                 std::next (decomposition.strongest.begin(), numUsed));

        if (shapeVersion != shownShapeVersion)
            grid.setShape (synthProcessor.getShape());

        grid.setPartials (partials);
        legend.setPartials (std::move (partials), numPartials);
        harmonicSelector.setNumAvailable (decomposition.numStrongest);

        for (size_t i = 0; i < presetButtons.size(); ++i)
            presetButtons[i].setToggleState (synthProcessor.getPreset() == waveform::allPresets[i],
                                             juce::dontSendNotification);

        if (numPartials != shownNumPartials)
            repaint (harmonicsHint);

        shownShapeVersion = shapeVersion;
        shownNumPartials = numPartials;
    }

    if (const auto note = synthProcessor.getLastNote(); note != shownNote)
    {
        shownNote = note;
        legend.setFundamental (note >= 0 ? std::optional (juce::MidiMessage::getMidiNoteInHertz (note)) : std::nullopt);
        repaint (noteArea);
    }
}

void SynthEditor::timerCallback()
{
    refresh();

    // In the standalone app the computer keyboard plays the piano straight away.
    if (! focusGiven && isShowing() && synthProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        keyboard.grabKeyboardFocus();
        focusGiven = true;
    }
}
