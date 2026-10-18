#include "SynthEditor.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));
}

SynthEditor::SynthEditor (SynthProcessor& p)
    : AudioProcessorEditor (p),
      synthProcessor (p),
      harmonicSelector (*p.parameters.getParameter (SynthProcessor::harmonicsId (p.editedOscillator))),
      volumeAttachment (p.parameters, SynthProcessor::volumeId, volumeKnob),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    for (size_t i = 0; i < cards.size(); ++i)
    {
        auto& card = cards[i];
        card = std::make_unique<OscillatorCard> ((int) i, p.parameters);
        card->onSelect = [this, i] { editOscillator ((int) i); };
        addAndMakeVisible (*card);
    }

    grid.onDraw = [this] (const waveform::Shape& shape)
    {
        synthProcessor.setShape (synthProcessor.editedOscillator, shape);
        refresh();
    };

    harmonicSliders.onAmplitudeChange = [this] (int harmonic, float amplitude)
    {
        synthProcessor.setHarmonicAmplitude (synthProcessor.editedOscillator, harmonic, amplitude);
        refresh();
    };

    addAndMakeVisible (grid);
    addAndMakeVisible (harmonicSliders);
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
            synthProcessor.setShape (synthProcessor.editedOscillator, waveform::makePreset (preset), preset);
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
    setResizeLimits (960, 700, 1800, 1200);
    setSize (1100, 820);

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

    const auto edited = synthProcessor.editedOscillator;
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("OSC " + juce::String (edited + 1) + " HARMONICS", harmonicsCaption, juce::Justification::centredLeft);
    g.drawText ("VOLUME", volumeCaption, juce::Justification::centred);

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

    area.removeFromTop (12);

    constexpr auto cardGap = 12;
    auto cardRow = area.removeFromTop (84);
    const auto cardWidth = (cardRow.getWidth() - cardGap * (SynthProcessor::numOscillators - 1)) / SynthProcessor::numOscillators;

    for (auto& card : cards)
    {
        card->setBounds (cardRow.removeFromLeft (cardWidth));
        cardRow.removeFromLeft (cardGap);
    }

    area.removeFromTop (14);

    auto bottom = area.removeFromBottom (juce::jlimit (110, 150, getHeight() / 5));
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
    side.removeFromTop (14);
    harmonicSliders.setBounds (side);
}

void SynthEditor::editOscillator (int oscillator)
{
    synthProcessor.editedOscillator = oscillator;
    refresh();
}

std::vector<waveform::Partial> SynthEditor::getPlayedHarmonics (int oscillator) const
{
    const auto& harmonics = synthProcessor.getDecomposition (oscillator).harmonics;
    const auto numHarmonics = juce::jlimit (1, waveform::maxHarmonics, synthProcessor.getNumHarmonics (oscillator));
    return { harmonics.begin(), std::next (harmonics.begin(), numHarmonics) };
}

juce::String SynthEditor::getWaveName (int oscillator) const
{
    const auto preset = synthProcessor.getPreset (oscillator);
    return preset.has_value() ? waveform::getName (*preset) : juce::String ("Custom");
}

void SynthEditor::refresh()
{
    Shown now;
    now.shapeVersion = synthProcessor.getShapeVersion();
    now.editedOscillator = synthProcessor.editedOscillator;

    for (size_t i = 0; i < cards.size(); ++i)
    {
        now.numHarmonics[i] = synthProcessor.getNumHarmonics ((int) i);
        now.on[i] = synthProcessor.isOn ((int) i);
    }

    if (now != shown)
    {
        const auto edited = now.editedOscillator;

        for (size_t i = 0; i < cards.size(); ++i)
        {
            cards[i]->setWave (getPlayedHarmonics ((int) i), getWaveName ((int) i));
            cards[i]->setSelected ((int) i == edited);
        }

        if (edited != shown.editedOscillator)
            harmonicSelector.setParameter (*synthProcessor.parameters.getParameter (SynthProcessor::harmonicsId (edited)));

        if (edited != shown.editedOscillator || now.shapeVersion != shown.shapeVersion)
            grid.setShape (synthProcessor.getShape (edited));

        const auto harmonics = getPlayedHarmonics (edited);
        grid.setPartials (harmonics);
        grid.setOscillator (edited + 1, now.on[(size_t) edited]);
        harmonicSliders.setHarmonics (harmonics);

        std::array<bool, waveform::maxHarmonics> silent {};

        for (const auto& harmonic : synthProcessor.getDecomposition (edited).harmonics)
            silent[(size_t) harmonic.harmonic - 1] = harmonic.amplitude <= 0.0f;

        harmonicSelector.setSilentHarmonics (silent);

        for (size_t i = 0; i < presetButtons.size(); ++i)
            presetButtons[i].setToggleState (synthProcessor.getPreset (edited) == waveform::allPresets[i],
                                             juce::dontSendNotification);

        repaint (harmonicsCaption);
        shown = now;
    }

    if (const auto note = synthProcessor.getLastNote(); note != shownNote)
    {
        shownNote = note;
        harmonicSliders.setFundamental (note >= 0 ? std::optional (juce::MidiMessage::getMidiNoteInHertz (note)) : std::nullopt);
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
