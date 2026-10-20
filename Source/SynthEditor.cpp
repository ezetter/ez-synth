#include "SynthEditor.h"

#include "ModulationEditor.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));
}

SynthEditor::SynthEditor (SynthProcessor& p)
    : AudioProcessorEditor (p),
      synthProcessor (p),
      lfoPanel (p),
      filterPanel (p.parameters),
      harmonicSelector (*p.parameters.getParameter (SynthProcessor::harmonicsId (0))),
      presetBar (p),
      volumeAttachment (p.parameters, SynthProcessor::volumeId, volumeKnob),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    for (size_t i = 0; i < oscillatorCards.size(); ++i)
    {
        auto& card = oscillatorCards[i];
        card = std::make_unique<OscillatorCard> ((int) i, p.parameters);
        card->onSelect = [this, i] { editSource ((int) i); };
        addAndMakeVisible (*card);
    }

    for (size_t i = 0; i < lfoCards.size(); ++i)
    {
        auto& card = lfoCards[i];
        card = std::make_unique<LfoCard> ((int) i, p.parameters);
        card->onSelect = [this, i] { editSource (SynthProcessor::numOscillators + (int) i); };
        addAndMakeVisible (*card);
    }

    grid.onDraw = [this] (const waveform::Shape& shape) { setEditedWave (shape, std::nullopt); };
    grid.onDrawingChange = [this] (const waveform::Drawing& drawing)
    {
        synthProcessor.setDrawing (synthProcessor.editedSource, drawing);
        refresh();
    };

    harmonicSliders.onAmplitudeChange = [this] (int harmonic, float amplitude)
    {
        synthProcessor.setHarmonicAmplitude (synthProcessor.editedSource, harmonic, amplitude);
        refresh();
    };

    addAndMakeVisible (grid);
    addChildComponent (harmonicSliders);
    addChildComponent (tuneSlider);
    addChildComponent (harmonicSelector);
    addChildComponent (lfoPanel);

    for (size_t i = 0; i < presetButtons.size(); ++i)
    {
        const auto preset = waveform::allPresets[i];
        auto& button = presetButtons[i];
        button.setButtonText (waveform::getName (preset));
        button.setMouseClickGrabsKeyboardFocus (false);
        button.setWantsKeyboardFocus (false);
        button.onClick = [this, preset] { setEditedWave (waveform::makePreset (preset), preset); };
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
    modulatable::setTarget (volumeKnob, modulation::volumeTarget);
    addAndMakeVisible (volumeKnob);

    for (size_t i = 0; i < envelopePanels.size(); ++i)
    {
        envelopePanels[i] = std::make_unique<EnvelopePanel> (p, (int) i);
        addAndMakeVisible (*envelopePanels[i]);
    }

    addAndMakeVisible (filterPanel);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (keyboard);

    if (MidiInputs::get() != nullptr)
    {
        midiPanel = std::make_unique<MidiPanel>();
        addAndMakeVisible (*midiPanel);
    }

    setResizable (true, true);
    setResizeLimits (1240, 900, 2000, 1400);
    setSize (1320, 960);

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

    const auto edited = synthProcessor.editedSource;
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);

    if (! SynthProcessor::isLfo (edited))
    {
        g.drawText ("OSC " + juce::String (edited + 1) + " HARMONICS", harmonicsCaption, juce::Justification::centredLeft);
        g.setColour (colours::outline);
        g.fillRect (tuneDivider);
        g.setColour (colours::dimText);
    }

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

    // The sound's preset sits between the waves and the note being played.
    presets.removeFromLeft (16);
    presets.removeFromRight (16);
    presetBar.setBounds (presets.withWidth (juce::jmin (presets.getWidth(), 420)).withX (presets.getRight() - juce::jmin (presets.getWidth(), 420)));

    area.removeFromTop (12);

    // The oscillators, then the LFOs, with a wider gap between the two.
    constexpr auto cardGap = 10;
    constexpr auto groupGap = 24;
    auto cardRow = area.removeFromTop (108);
    const auto cardWidth = (cardRow.getWidth() - groupGap - cardGap * (SynthProcessor::numSources - 2)) / SynthProcessor::numSources;

    for (int i = 0; i < SynthProcessor::numSources; ++i)
    {
        SourceCard& card = SynthProcessor::isLfo (i) ? static_cast<SourceCard&> (*lfoCards[(size_t) (i - SynthProcessor::numOscillators)])
                                                     : *oscillatorCards[(size_t) i];
        card.setBounds (cardRow.removeFromLeft (cardWidth));
        cardRow.removeFromLeft (i == SynthProcessor::numOscillators - 1 ? groupGap : cardGap);
    }

    area.removeFromTop (14);

    // Along the bottom, the keyboard, with the MIDI devices beside it in the standalone app, and above them the volume,
    // the envelopes and the filters.
    constexpr auto rowGap = 12;
    auto keys = area.removeFromBottom (juce::jlimit (90, 130, getHeight() / 8));

    if (midiPanel != nullptr)
    {
        midiPanel->setBounds (keys.removeFromLeft (120));
        keys.removeFromLeft (rowGap);
    }

    keyboard.setBounds (keys);
    area.removeFromBottom (12);

    auto controls = area.removeFromBottom (FilterPanel::preferredHeight);
    volumePanel = controls.removeFromLeft (120);
    controls.removeFromLeft (rowGap);
    filterPanel.setBounds (controls.removeFromRight (380));
    controls.removeFromRight (rowGap);

    const auto envelopeWidth = (controls.getWidth() - rowGap * (SynthProcessor::numEnvelopes - 1)) / SynthProcessor::numEnvelopes;

    for (auto& panel : envelopePanels)
    {
        panel->setBounds (controls.removeFromLeft (envelopeWidth));
        controls.removeFromLeft (rowGap);
    }

    auto volume = volumePanel.reduced (10, 10);
    volumeCaption = volume.removeFromTop (16);
    volumeValue = volume.removeFromBottom (20);
    volumeKnob.setBounds (volume);

    area.removeFromBottom (14);

    // Beside the grid, the chosen oscillator's harmonics, or what the chosen LFO moves.
    sidePanel = area.removeFromRight (380);
    area.removeFromRight (14);
    grid.setBounds (area);

    auto side = sidePanel.reduced (16, 14);
    lfoPanel.setBounds (side);
    harmonicsCaption = side.removeFromTop (18);
    side.removeFromTop (6);
    harmonicSelector.setBounds (side.removeFromTop (34));
    side.removeFromTop (10);

    // The oscillator's tune, at the foot of the panel. The spacing is tight, so the smallest window still has room
    // for all nine harmonic sliders.
    tuneSlider.setBounds (side.removeFromBottom (TuneSlider::preferredHeight));
    side.removeFromBottom (8);
    tuneDivider = side.removeFromBottom (1);
    side.removeFromBottom (7);
    harmonicSliders.setBounds (side);
}

void SynthEditor::editSource (int source)
{
    synthProcessor.editedSource = source;
    refresh();
}

void SynthEditor::setEditedWave (const waveform::Shape& shape, std::optional<waveform::Preset> preset)
{
    const auto edited = synthProcessor.editedSource;

    if (SynthProcessor::isLfo (edited))
        synthProcessor.setLfoShape (edited - SynthProcessor::numOscillators, shape, preset);
    else
        synthProcessor.setShape (edited, shape, preset);

    refresh();
}

std::vector<waveform::Partial> SynthEditor::getPlayedHarmonics (int oscillator) const
{
    const auto& harmonics = synthProcessor.getDecomposition (oscillator).harmonics;
    const auto numHarmonics = juce::jlimit (1, waveform::maxHarmonics, synthProcessor.getNumHarmonics (oscillator));
    return { harmonics.begin(), std::next (harmonics.begin(), numHarmonics) };
}

juce::String SynthEditor::getWaveName (std::optional<waveform::Preset> preset)
{
    return preset.has_value() ? waveform::getName (*preset) : juce::String ("Custom");
}

void SynthEditor::refresh()
{
    Shown now;
    now.shapeVersion = synthProcessor.getShapeVersion();
    now.modulationVersion = synthProcessor.getModulationVersion();
    now.editedSource = synthProcessor.editedSource;

    for (int i = 0; i < SynthProcessor::numOscillators; ++i)
    {
        now.numHarmonics[(size_t) i] = synthProcessor.getNumHarmonics (i);
        now.tuneCents[(size_t) i] = juce::roundToInt (synthProcessor.getTune (i) * 100.0f);
        now.on[(size_t) i] = synthProcessor.isOn (i);
    }

    for (int i = 0; i < SynthProcessor::numLfos; ++i)
        now.on[(size_t) (SynthProcessor::numOscillators + i)] = synthProcessor.isLfoOn (i);

    for (int i = 0; i < SynthProcessor::numSources; ++i)
        now.drawings[(size_t) i] = synthProcessor.getDrawing (i);

    const auto edited = now.editedSource;
    const auto editingLfo = SynthProcessor::isLfo (edited);
    const auto lfo = edited - SynthProcessor::numOscillators;

    const auto changed = now != shown;

    if (changed)
    {
        for (size_t i = 0; i < oscillatorCards.size(); ++i)
            oscillatorCards[i]->setSelected ((int) i == edited);

        for (size_t i = 0; i < lfoCards.size(); ++i)
        {
            lfoCards[i]->setWave (synthProcessor.getLfoShape ((int) i), getWaveName (synthProcessor.getLfoPreset ((int) i)));
            lfoCards[i]->setSelected ((int) i == lfo);
        }

        const auto preset = editingLfo ? synthProcessor.getLfoPreset (lfo) : synthProcessor.getPreset (edited);

        for (size_t i = 0; i < presetButtons.size(); ++i)
            presetButtons[i].setToggleState (preset == waveform::allPresets[i], juce::dontSendNotification);

        grid.setDrawing (synthProcessor.getDrawing (edited));
        harmonicSelector.setVisible (! editingLfo);
        harmonicSliders.setVisible (! editingLfo);
        tuneSlider.setVisible (! editingLfo);
        lfoPanel.setVisible (editingLfo);

        if (editingLfo)
        {
            grid.setShape (synthProcessor.getLfoShape (lfo));
            grid.setPartials ({});
            lfoPanel.setLfo (lfo);
        }
        else
        {
            if (edited != shown.editedSource)
            {
                harmonicSelector.setParameter (*synthProcessor.parameters.getParameter (SynthProcessor::harmonicsId (edited)));
                modulatable::setTarget (harmonicSelector, modulation::oscillatorHarmonicsTarget (edited));
                harmonicSliders.setOscillator (edited);
                tuneSlider.setOscillator (*synthProcessor.parameters.getParameter (SynthProcessor::tuneId (edited)), edited);
            }

            const auto harmonics = getPlayedHarmonics (edited);
            grid.setShape (synthProcessor.getShape (edited));
            grid.setPartials (harmonics);
            grid.setOscillator (edited + 1, now.on[(size_t) edited]);
            harmonicSliders.setHarmonics (harmonics);

            std::array<bool, waveform::maxHarmonics> silent {};

            for (const auto& harmonic : synthProcessor.getDecomposition (edited).harmonics)
                silent[(size_t) harmonic.harmonic - 1] = harmonic.amplitude <= 0.0f;

            harmonicSelector.setSilentHarmonics (silent);
        }

        repaint (sidePanel);
        shown = now;

        // Controls may now set other targets, such as the harmonic sliders when another oscillator is chosen.
        showModulation();
    }

    // The oscillators' waves as the newest note plays them, which LFOs, envelopes and velocity can change all the time.
    // The grid draws its oscillator's harmonics, and their sum, where they've been moved to, over the wave as drawn.
    for (int i = 0; i < SynthProcessor::numOscillators; ++i)
    {
        auto live = synthProcessor.getLiveWave (i);

        if (! changed && live == liveWaves[(size_t) i])
            continue;

        oscillatorCards[(size_t) i]->setWave (live.harmonics, getWaveName (synthProcessor.getPreset (i)), synthProcessor.getTune (i));

        if (i == edited)
            grid.setMovedPartials (live.moved ? std::optional (live.harmonics) : std::nullopt);

        liveWaves[(size_t) i] = std::move (live);
    }

    // An LFO's period and where it has got to change all the time, so they're shown on every refresh.
    if (editingLfo)
    {
        const auto once = synthProcessor.isLfoOnce (lfo);
        grid.setLfo (lfo + 1, synthProcessor.isLfoOn (lfo), synthProcessor.getLfoPeriod (lfo), once);

        if (const auto progress = synthProcessor.getLfoProgress (lfo))
        {
            // One that runs once stays at the end of its period once it's got there.
            const auto position = once ? std::min (*progress, 1.0) : *progress - std::floor (*progress);
            grid.setPlayhead ((float) position, synthProcessor.getLfoValue (lfo));
        }
        else
        {
            grid.setPlayhead (std::nullopt);
        }
    }

    for (auto& panel : envelopePanels)
        panel->refresh();

    presetBar.refresh();

    if (midiPanel != nullptr)
        midiPanel->refresh (synthProcessor.getMidiReceived());

    // The harmonics' pitches are those of the note, raised by as much as the oscillator is tuned up.
    if (const auto note = synthProcessor.getLastNote(); note != shownNote || changed)
    {
        if (note != shownNote)
            repaint (noteArea);

        shownNote = note;
        const auto tune = editingLfo ? 0.0f : synthProcessor.getTune (edited);
        harmonicSliders.setFundamental (note >= 0 ? std::optional (juce::MidiMessage::getMidiNoteInHertz (note) * tuning::toRatio (tune))
                                                  : std::nullopt);
    }
}

void SynthEditor::showModulation()
{
    // Controls that are hidden, such as the harmonic sliders while an LFO is shown, are left alone.
    std::function<void (juce::Component&, bool)> show = [&] (juce::Component& component, bool parentVisible)
    {
        const auto visible = parentVisible && component.isVisible();

        if (const auto target = modulatable::getTarget (component))
            modulatable::setLivePosition (component, visible ? synthProcessor.getLivePosition (*target) : std::nullopt);

        for (auto* child : component.getChildren())
            show (*child, visible);
    };

    // The window itself counts as visible, as it is to whoever's looking at it, even before a host shows it.
    for (auto* child : getChildren())
        show (*child, true);
}

void SynthEditor::showModulationEditor (juce::Component& control)
{
    const auto target = modulatable::getTarget (control);

    if (! target.has_value())
        return;

    juce::CallOutBox::launchAsynchronously (std::make_unique<ModulationEditor> (synthProcessor, *target),
                                            getLocalArea (&control, control.getLocalBounds()), this);
}

void SynthEditor::timerCallback()
{
    refresh();
    showModulation();

    // In the standalone app the computer keyboard plays the piano straight away.
    if (! focusGiven && isShowing() && synthProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        keyboard.grabKeyboardFocus();
        focusGiven = true;
    }
}
