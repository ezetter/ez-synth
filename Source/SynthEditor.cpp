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
        oscillatorStrip.addCard (*card);
    }

    addAndMakeVisible (oscillatorStrip);

    for (size_t i = 0; i < lfoCards.size(); ++i)
    {
        auto& card = lfoCards[i];
        card = std::make_unique<LfoCard> ((int) i, p.parameters);
        card->onSelect = [this, i] { editSource (SynthProcessor::numOscillators + (int) i); };
        lfoStrip.addCard (*card);
    }

    addAndMakeVisible (lfoStrip);

    grid.onDraw = [this] (const waveform::Shape& shape) { setEditedWave (shape, std::nullopt); };
    grid.onDrawingChange = [this] (const waveform::Drawing& drawing)
    {
        synthProcessor.setDrawing (synthProcessor.editedSource, drawing);
        refresh();
    };

    grid.onRandomChange = [this] (bool random)
    {
        if (! SynthProcessor::isLfo (synthProcessor.editedSource))
            return;

        auto& parameter = *synthProcessor.parameters.getParameter (SynthProcessor::lfoRandomId (synthProcessor.editedSource - SynthProcessor::numOscillators));
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (random ? 1.0f : 0.0f);
        parameter.endChangeGesture();
        refresh();
    };

    grid.onNoiseChange = [this] (bool noise)
    {
        if (SynthProcessor::isLfo (synthProcessor.editedSource))
            return;

        auto& parameter = *synthProcessor.parameters.getParameter (SynthProcessor::noiseId (synthProcessor.editedSource));
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (noise ? 1.0f : 0.0f);
        parameter.endChangeGesture();
        refresh();
    };

    // Noise like an oscillator's, to show while no note plays it, the same each time.
    juce::Random stillRandom (20261115);

    for (int i = 0; i < SynthProcessor::noiseSamples; ++i)
        stillNoise.push_back (stillRandom.nextFloat() * 2.0f - 1.0f);

    harmonicSliders.onAmplitudeChange = [this] (int harmonic, float amplitude)
    {
        synthProcessor.setHarmonicAmplitude (synthProcessor.editedSource, harmonic, amplitude);
        refresh();
    };

    harmonicSliders.onTuneChange = [this] (int harmonic, float tune)
    {
        synthProcessor.setHarmonicTune (synthProcessor.editedSource, harmonic, tune);
        refresh();
    };

    harmonicSliders.onPhaseChange = [this] (int harmonic, float phase)
    {
        synthProcessor.setHarmonicPhase (synthProcessor.editedSource, harmonic, phase);
        refresh();
    };

    addAndMakeVisible (grid);
    addChildComponent (harmonicSliders);
    addChildComponent (oscillatorPanel);
    addChildComponent (harmonicSelector);
    addChildComponent (lfoPanel);

    // Each click on a harmonic's box steps it on: following the slider, then off, then on, then following again.
    harmonicSelector.onToggle = [this] (int harmonic)
    {
        userTouched = true;
        synthProcessor.cycleHarmonicSwitch (synthProcessor.editedSource, harmonic);
        refresh();
    };

    randomizeButton.setTooltip ("Give this oscillator's 16 harmonics random amplitudes and phases. Right-click to have an LFO, "
                                "envelope, velocity or the mod wheel randomize them each time it crosses its midline.");
    randomizeButton.setMouseClickGrabsKeyboardFocus (false);
    randomizeButton.setWantsKeyboardFocus (false);
    randomizeButton.onClick = [this]
    {
        userTouched = true;
        synthProcessor.randomizeHarmonics (synthProcessor.editedSource, buttonRandom);
        refresh();
    };
    addChildComponent (randomizeButton);

    grid.onPresetChosen = [this] (waveform::Preset preset) { setEditedWave (waveform::makePreset (preset), preset); };

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
        envelopeStrip.addCard (*envelopePanels[i]);
    }

    addAndMakeVisible (envelopeStrip);

    for (size_t i = 0; i < filterPanels.size(); ++i)
    {
        filterPanels[i] = std::make_unique<FilterPanel> (p.parameters, (int) i);
        filterStrip.addCard (*filterPanels[i]);
    }

    addAndMakeVisible (filterStrip);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (keyboard);

    if (MidiInputs::get() != nullptr)
    {
        midiPanel = std::make_unique<MidiPanel>();
        addAndMakeVisible (*midiPanel);
    }


    modulationOverview.onEdit = [this] (int target, juce::Component& row) { showModulationEditor (target, row); };
    modulationOverview.onClose = [this] { showModulationOverview (false); };
    addChildComponent (modulationOverview);


    pitchSetScreen.onClose = [this] { showPitchSetScreen (false); };

    // While it waits for keys, the computer's keys play the keyboard, as they do in the standalone app.
    pitchSetScreen.onWaitingForKeys = [this] (bool waiting)
    {
        if (waiting)
            keyboard.grabKeyboardFocus();
        else if (pitchSetScreen.isShowing())
            pitchSetScreen.grabKeyboardFocus();
    };
    addChildComponent (pitchSetScreen);

    // Choosing another oscillator on the routing screen edits it in the window too.
    routingScreen.onClose = [this] { showRoutingScreen (false); };
    routingScreen.onOscillatorChosen = [this] (int oscillator) { editSource (oscillator); };
    addChildComponent (routingScreen);
    oscillatorPanel.onShowFilterRouting = [this] { showRoutingScreen (true, synthProcessor.editedSource); };
    routingScreen.onShowEffects = [this] { showEffectsScreen (true); };

    effectsScreen.onClose = [this] { showEffectsScreen (false); };
    addChildComponent (effectsScreen);

    // The tabs along the top: the synth itself, and the screens over it.
    tabs.setTitle ("Screens");
    tabs.setTabTooltip ((int) Tab::synth, "The synth: its oscillators, LFOs, envelopes and filters");
    tabs.setTabTooltip ((int) Tab::effects, "Set the delay and reverb, and which oscillators are sent through them");
    tabs.setTabTooltip ((int) Tab::pitchSets, "Set the sound differently for ranges of keys");
    tabs.setTabTooltip ((int) Tab::modulation, "See everything that moves a control, or is linked to a MIDI controller, together");
    tabs.onChoose = [this] (int tab) { showTab ((Tab) tab); };
    addAndMakeVisible (tabs);

    setResizable (true, true);
    setResizeLimits (1420, 900, 2200, 1400);
    setSize (1440, 960);

    // Hears every click and scroll in the window, whichever control it lands on, and takes the keys none of them
    // use, such as Command-Z.
    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    refresh();
    rememberChanges();
    startTimerHz (30);
}

SynthEditor::~SynthEditor()
{
    removeMouseListener (this);
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

    for (const auto& panel : { sidePanel, oscillatorPanelArea, volumePanel })
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (panel.toFloat(), 10.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    const auto edited = synthProcessor.editedSource;
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);

    if (! SynthProcessor::isLfo (edited) && synthProcessor.isNoise (edited))
    {
        // White noise has no harmonics, pitch or glide to set.
        g.drawText ("OSC " + juce::String (edited + 1) + " WHITE NOISE", harmonicsCaption, juce::Justification::centredLeft);
        g.setColour (colours::dimText);
        g.setFont (fonts::body (14.0f));
        g.drawFittedText ("Every frequency at once, with a new random value every sample, so there are no harmonics, "
                          "tune or glide to set.\n\nIts level and switch still count, as do the envelopes and LFOs on them. "
                          "Shape its sound with the filters.\n\nSwitch Noise off, above the wave, to play harmonics again.",
                          sidePanel.reduced (16, 14).withTop (harmonicsCaption.getBottom() + 14), juce::Justification::topLeft, 12);
    }
    else if (! SynthProcessor::isLfo (edited))
    {
        g.drawText ("OSC " + juce::String (edited + 1) + " HARMONICS", harmonicsCaption, juce::Justification::centredLeft);
    }

    // Beside the wave, the oscillator's pan, unison, tune, glide and routing, which an LFO doesn't have, so it says so.
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);

    if (SynthProcessor::isLfo (edited))
    {
        g.drawText ("LFO " + juce::String (edited - SynthProcessor::numOscillators + 1), oscillatorCaption, juce::Justification::centredLeft);
        g.setFont (fonts::body (13.0f));
        g.drawFittedText ("An LFO has no pan, unison, tune, glide or routing. Choose an oscillator's card to set its own.",
                          oscillatorPanelArea.reduced (14).withTop (oscillatorCaption.getBottom() + 10), juce::Justification::topLeft, 6);
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
    noteArea = header.removeFromRight (220);

    // The sound's preset after the title, and the tabs before the note being played, along the foot of the header.
    auto row = header.withSizeKeepingCentre (header.getWidth(), 34);
    row.removeFromRight (20);
    tabs.setBounds (row.removeFromRight (juce::jmin (tabs.getPreferredWidth() + 24, row.getWidth() - 400)).withHeight (header.getBottom() - row.getY()));
    row.removeFromRight (24);
    row.removeFromLeft (8);
    presetBar.setBounds (row.withWidth (juce::jmin (row.getWidth(), 560)));
    const auto screensTop = header.getBottom() + 6;

    area.removeFromTop (12);

    // Along the bottom, the keyboard, with the MIDI devices beside it in the standalone app.
    constexpr auto rowGap = 12;
    auto keys = area.removeFromBottom (juce::jlimit (90, 130, getHeight() / 8));

    if (midiPanel != nullptr)
    {
        midiPanel->setBounds (keys.removeFromLeft (120));
        keys.removeFromLeft (rowGap);
    }

    keyboard.setBounds (keys);
    pitchSetScreen.setBounds (getLocalBounds().withTop (screensTop).withBottom (keys.getY() - 6));
    routingScreen.setBounds (pitchSetScreen.getBounds());
    effectsScreen.setBounds (pitchSetScreen.getBounds());
    modulationOverview.setBounds (pitchSetScreen.getBounds());
    area.removeFromBottom (12);

    // Down the right, from the cards to the filters, the chosen oscillator's harmonics, or what the chosen LFO moves,
    // tall enough for big dials.
    sidePanel = area.removeFromRight (juce::jlimit (380, 460, area.getWidth() * 27 / 100));
    area.removeFromRight (14);

    // The oscillators, then the LFOs, with a wider gap between the two. Three of each show at a time, each row
    // scrolling sideways to the others, with its scroll bar in the gap under the cards.
    constexpr auto cardGap = 8;
    constexpr auto groupGap = 18;
    constexpr auto underCards = 14;
    auto cardRow = area.removeFromTop (108 + underCards);
    const auto stripWidth = (cardRow.getWidth() - groupGap) / 2;
    oscillatorStrip.setGaps (cardGap, underCards);
    oscillatorStrip.setBounds (cardRow.removeFromLeft (stripWidth));
    cardRow.removeFromLeft (groupGap);
    lfoStrip.setGaps (cardGap, underCards);
    lfoStrip.setBounds (cardRow);

    // Under the grid, the volume, the envelopes and the filters.
    constexpr auto controlGap = 10;
    auto controls = area.removeFromBottom (FilterPanel::preferredHeight);
    volumePanel = controls.removeFromLeft (90);
    controls.removeFromLeft (controlGap);

    // Two filters, and three envelopes, show at a time, each row scrolling sideways to the others, with its scroll
    // bar in the gap under it.
    constexpr auto underEnvelopes = 12;
    filterStrip.setGaps (controlGap, underEnvelopes);
    filterStrip.setBounds (controls.removeFromRight (juce::jlimit (300, 400, controls.getWidth() * 9 / 25))
                                   .withHeight (controls.getHeight() + underEnvelopes));
    controls.removeFromRight (controlGap);
    envelopeStrip.setGaps (controlGap, underEnvelopes);
    envelopeStrip.setBounds (controls.withHeight (controls.getHeight() + underEnvelopes));

    auto volume = volumePanel.reduced (10, 10);
    volumeCaption = volume.removeFromTop (16);
    volumeValue = volume.removeFromBottom (20);
    volumeKnob.setBounds (volume);

    area.removeFromBottom (14);

    // Beside the wave, the chosen oscillator's pan, unison, tune, glide and routing, leaving the wave room for the
    // controls along its top.
    constexpr auto gridRoom = 716;
    oscillatorPanelArea = area.removeFromLeft (juce::jlimit (256, 360, area.getWidth() - 14 - gridRoom));
    area.removeFromLeft (14);
    grid.setBounds (area);

    // The panel's sections, which say which oscillator they're for no more than the wave beside them does.
    const auto oscillatorSide = oscillatorPanelArea.reduced (14, 12);
    oscillatorCaption = oscillatorSide.withHeight (18);
    oscillatorPanel.setBounds (oscillatorSide);

    auto side = sidePanel.reduced (16, 14);
    lfoPanel.setBounds (side);
    harmonicsCaption = side.removeFromTop (24);
    randomizeButton.setBounds (harmonicsCaption.removeFromRight (100));
    side.removeFromTop (6);
    harmonicSelector.setBounds (side.removeFromTop (HarmonicSelector::preferredHeight));
    side.removeFromTop (10);
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
    {
        const auto lfo = edited - SynthProcessor::numOscillators;
        synthProcessor.setLfoShape (lfo, shape, preset);

        // Choosing a wave for an LFO playing a random one means it should play that wave instead.
        if (synthProcessor.isLfoRandom (lfo))
        {
            auto& random = *synthProcessor.parameters.getParameter (SynthProcessor::lfoRandomId (lfo));
            random.beginChangeGesture();
            random.setValueNotifyingHost (0.0f);
            random.endChangeGesture();
        }
    }
    else
    {
        synthProcessor.setShape (edited, shape, preset);
    }

    refresh();
}

std::vector<waveform::Partial> SynthEditor::getPlayedHarmonics (int oscillator) const
{
    const auto& harmonics = synthProcessor.getDecomposition (oscillator).harmonics;
    std::vector<waveform::Partial> played;

    for (const auto& harmonic : harmonics)
        if (synthProcessor.isHarmonicPlayed (oscillator, harmonic.harmonic))
            played.push_back (harmonic);

    for (auto& partial : played)
    {
        partial.tune = synthProcessor.getHarmonicTune (oscillator, partial.harmonic);
        partial.phase = synthProcessor.getHarmonicPhase (oscillator, partial.harmonic);
    }

    return played;
}

juce::String SynthEditor::getWaveName (std::optional<waveform::Preset> preset)
{
    return preset.has_value() ? waveform::getName (*preset) : juce::String ("Custom");
}

juce::String SynthEditor::getLfoWaveName (int lfo) const
{
    if (synthProcessor.isLfoRandom (lfo))
        return synthProcessor.getDrawing (SynthProcessor::numOscillators + lfo).stepped ? "Random steps" : "Random";

    return getWaveName (synthProcessor.getLfoPreset (lfo));
}

void SynthEditor::refresh()
{
    showPitchSets();

    Shown now;
    now.shapeVersion = synthProcessor.getShapeVersion();
    now.modulationVersion = synthProcessor.getModulationVersion();
    now.editedSource = synthProcessor.editedSource;

    for (int i = 0; i < SynthProcessor::numOscillators; ++i)
    {
        now.numHarmonics[(size_t) i] = synthProcessor.getNumHarmonics (i);
        now.tuneCents[(size_t) i] = juce::roundToInt (synthProcessor.getTune (i) * 100.0f);
        now.on[(size_t) i] = synthProcessor.isOn (i);
        now.noise[(size_t) i] = synthProcessor.isNoise (i);
    }

    for (int i = 0; i < SynthProcessor::numLfos; ++i)
        now.on[(size_t) (SynthProcessor::numOscillators + i)] = synthProcessor.isLfoOn (i);

    for (int i = 0; i < SynthProcessor::numSources; ++i)
        now.drawings[(size_t) i] = synthProcessor.getDrawing (i);

    for (int i = 0; i < SynthProcessor::numLfos; ++i)
        now.random[(size_t) i] = synthProcessor.isLfoRandom (i);

    const auto edited = now.editedSource;
    const auto editingLfo = SynthProcessor::isLfo (edited);
    const auto lfo = edited - SynthProcessor::numOscillators;

    const auto changed = now != shown;

    // The oscillator or LFO being edited is always in sight, such as when the window opens on it.
    if (now.editedSource != shown.editedSource)
    {
        if (editingLfo)
            lfoStrip.scrollToShow (lfo);
        else
            oscillatorStrip.scrollToShow (edited);
    }

    if (changed)
    {
        for (size_t i = 0; i < oscillatorCards.size(); ++i)
            oscillatorCards[i]->setSelected ((int) i == edited);

        for (size_t i = 0; i < lfoCards.size(); ++i)
        {
            lfoCards[i]->setWave (synthProcessor.getPlayedLfoShape ((int) i), getLfoWaveName ((int) i), synthProcessor.getPlayedLfoEnd ((int) i));
            lfoCards[i]->setSelected ((int) i == lfo);
        }

        // An LFO playing a random wave isn't playing any of these.
        const auto preset = editingLfo ? (synthProcessor.isLfoRandom (lfo) ? std::nullopt : synthProcessor.getLfoPreset (lfo))
                                       : synthProcessor.getPreset (edited);

        grid.setPreset (preset);

        grid.setDrawing (synthProcessor.getDrawing (edited));

        // An oscillator playing noise has none of its harmonics' controls.
        const auto harmonicsShown = ! editingLfo && ! now.noise[(size_t) edited];
        harmonicSelector.setVisible (harmonicsShown);
        randomizeButton.setVisible (harmonicsShown);
        harmonicSliders.setVisible (harmonicsShown);
        // A noise oscillator has no tune or glide, but it's still panned.
        oscillatorPanel.setVisible (! editingLfo);
        oscillatorPanel.setPanOnly (! harmonicsShown);
        lfoPanel.setVisible (editingLfo);
        grid.setNoise (editingLfo ? std::nullopt : std::optional (now.noise[(size_t) edited]));

        if (editingLfo)
        {
            grid.setRandom (synthProcessor.isLfoRandom (lfo));
            grid.setShape (synthProcessor.getPlayedLfoShape (lfo), synthProcessor.getPlayedLfoEnd (lfo));
            grid.setPartials ({});
            lfoPanel.setLfo (lfo);
        }
        else
        {
            if (edited != shown.editedSource)
            {
                harmonicSelector.setParameter (*synthProcessor.parameters.getParameter (SynthProcessor::harmonicsId (edited)),
                                               modulation::oscillatorHarmonicsTarget (edited));
                harmonicSelector.setOscillator (edited);
                harmonicSliders.setOscillator (edited);
                modulatable::setTarget (randomizeButton, modulation::randomizeTarget (edited));
                oscillatorPanel.setOscillator (synthProcessor.parameters, edited);
            }

            const auto harmonics = getPlayedHarmonics (edited);
            grid.setRandom (std::nullopt);
            grid.setShape (synthProcessor.getShape (edited));
            grid.setPartials (harmonics);
            grid.setOscillator (edited + 1, now.on[(size_t) edited]);
            harmonicSliders.setHarmonics (harmonics);

            std::array<bool, waveform::maxHarmonics> played {}, silent {};

            for (const auto& harmonic : synthProcessor.getDecomposition (edited).harmonics)
            {
                played[(size_t) harmonic.harmonic - 1] = synthProcessor.isHarmonicPlayed (edited, harmonic.harmonic);
                silent[(size_t) harmonic.harmonic - 1] = harmonic.amplitude <= 0.0f;
            }

            waveform::HarmonicSwitches switches {};

            for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
                switches[(size_t) harmonic - 1] = synthProcessor.getHarmonicSwitch (edited, harmonic);

            harmonicSelector.setHarmonics (played, silent, switches);

            // The button's text is in the modulation colour while something triggers it.
            const auto randomizeTarget = modulation::randomizeTarget (edited);
            const auto triggered = synthProcessor.getAssignment (randomizeTarget).isActive()
                                || synthProcessor.getVelocityAssignment (randomizeTarget).isActive()
                                || synthProcessor.getModWheelAssignment (randomizeTarget).isActive();
            randomizeButton.setColour (juce::TextButton::textColourOffId, triggered ? colours::modulation : colours::text);
        }

        repaint (sidePanel);
        repaint (oscillatorPanelArea);
        shown = now;

        // Controls may now set other targets, such as the harmonic sliders when another oscillator is chosen.
        showModulation();
    }

    // A random LFO plays a new wave every cycle, or with every key, which its card and the grid show.
    for (int i = 0; i < SynthProcessor::numLfos; ++i)
    {
        if (! synthProcessor.isLfoRandom (i))
            continue;

        const auto cycle = synthProcessor.getRandomCycle (i);

        if (! changed && cycle == randomCycles[(size_t) i])
            continue;

        randomCycles[(size_t) i] = cycle;
        const auto shape = synthProcessor.getPlayedLfoShape (i);
        const auto end = synthProcessor.getPlayedLfoEnd (i);
        lfoCards[(size_t) i]->setWave (shape, getLfoWaveName (i), end);

        if (editingLfo && i == lfo)
            grid.setShape (shape, end);
    }

    // The oscillators' waves as the newest note plays them, which LFOs, envelopes and velocity can change all the time.
    // The grid draws its oscillator's harmonics, and their sum, where they've been moved to, over the wave as drawn.
    for (int i = 0; i < SynthProcessor::numOscillators; ++i)
    {
        auto live = synthProcessor.getLiveWave (i);

        if (now.noise[(size_t) i] || (! changed && live == liveWaves[(size_t) i]))
            continue;

        oscillatorCards[(size_t) i]->setWave (live.harmonics, getWaveName (synthProcessor.getPreset (i)), synthProcessor.getTune (i));

        if (i == edited)
            grid.setMovedPartials (live.moved ? std::optional (live.harmonics) : std::nullopt);

        liveWaves[(size_t) i] = std::move (live);
    }

    showNoise (changed);

    // An LFO's period and where it has got to change all the time, so they're shown on every refresh.
    if (editingLfo)
    {
        const auto times = synthProcessor.getLfoTimes (lfo);
        grid.setLfo (lfo + 1, synthProcessor.isLfoOn (lfo), synthProcessor.getLfoPeriod (lfo), synthProcessor.isLfoFromKey (lfo),
                     synthProcessor.isLfoOnLastTime (lfo));

        if (const auto progress = synthProcessor.getLfoProgress (lfo))
        {
            // One that plays its wave a number of times stays at the end of its period once it's played the last.
            const auto finished = synthProcessor.isLfoFromKey (lfo) && times > 0 && *progress >= (double) times;
            const auto position = finished ? 1.0 : *progress - std::floor (*progress);
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

    if (routingScreen.isVisible())
        routingScreen.refresh();

    if (effectsScreen.isVisible())
        effectsScreen.refresh();

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

void SynthEditor::showNoise (bool changed)
{
    const auto count = synthProcessor.getNoiseCount();

    if (! changed && count == shownNoiseCount)
        return;

    shownNoiseCount = count;
    const auto edited = synthProcessor.editedSource;
    const auto seconds = SynthProcessor::noiseSamples / synthProcessor.getPlayingSampleRate();

    for (int i = 0; i < SynthProcessor::numOscillators; ++i)
    {
        if (! shown.noise[(size_t) i])
            continue;

        const auto played = synthProcessor.getPlayedNoise (i);
        const auto& samples = played.empty() ? stillNoise : played;
        oscillatorCards[(size_t) i]->setNoise (samples);

        if (i == edited)
            grid.setNoiseSamples (samples, seconds, ! played.empty());
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

    // An LFO's rate, as the mod wheel has moved it, is shown under its dial too.
    // Synced to the tempo, its dial sets how many cycles it runs a beat instead.
    for (int lfo = 0; lfo < SynthProcessor::numLfos; ++lfo)
    {
        lfoCards[(size_t) lfo]->showMovedRate (synthProcessor.getLivePosition (modulation::lfoRateTarget (lfo)));
        lfoCards[(size_t) lfo]->showSynced (synthProcessor.isLfoSynced (lfo));
    }

    lfoPanel.showTempo();
}

void SynthEditor::showModulationEditor (juce::Component& control)
{
    if (const auto target = modulatable::getTarget (control))
        showModulationEditor (*target, control);
}

void SynthEditor::showModulationEditor (int target, juce::Component& anchor)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<ModulationEditor> (synthProcessor, target),
                                            getLocalArea (&anchor, anchor.getLocalBounds()), this);
}

void SynthEditor::showTab (Tab tab)
{
    // Each tab but the synth's is a screen over the synth, under the header, so the tabs stay in reach, and over
    // everything but the keyboard. The routing screen belongs to the synth's tab, and goes when another's chosen.
    tabs.setChosen ((int) tab);
    routingScreen.setVisible (false);
    effectsScreen.setVisible (tab == Tab::effects);
    pitchSetScreen.setVisible (tab == Tab::pitchSets);
    modulationOverview.setVisible (tab == Tab::modulation);

    juce::Component* screen = nullptr;

    switch (tab)
    {
        case Tab::effects:      effectsScreen.refresh(); screen = &effectsScreen; break;
        case Tab::pitchSets:    screen = &pitchSetScreen; break;
        case Tab::modulation:   screen = &modulationOverview; break;
        case Tab::synth:        break;
    }

    if (screen != nullptr)
    {
        screen->toFront (true);
        screen->grabKeyboardFocus();
    }
    else if (isShowing())
    {
        grabKeyboardFocus();
    }
}

void SynthEditor::showRoutingScreen (bool visible, int oscillator)
{
    if (visible)
    {
        showTab (Tab::synth);
        routingScreen.showOscillator (oscillator);
        routingScreen.setVisible (true);
        routingScreen.toFront (true);
        routingScreen.grabKeyboardFocus();
    }
    else if (routingScreen.isVisible())
    {
        showTab (Tab::synth);
    }
}

void SynthEditor::showEffectsScreen (bool visible)
{
    if (visible)
        showTab (Tab::effects);
    else if (isEffectsScreenShown())
        showTab (Tab::synth);
}

void SynthEditor::showPitchSetScreen (bool visible)
{
    if (visible)
        showTab (Tab::pitchSets);
    else if (isPitchSetScreenShown())
        showTab (Tab::synth);
}

void SynthEditor::showModulationOverview (bool visible)
{
    if (visible)
        showTab (Tab::modulation);
    else if (isModulationOverviewShown())
        showTab (Tab::synth);
}

void SynthEditor::showPitchSets()
{
    if (synthProcessor.getPitchSetVersion() == shownPitchSetVersion)
        return;

    shownPitchSetVersion = synthProcessor.getPitchSetVersion();
    const auto active = synthProcessor.getActivePitchSet();

    // With others than the default, the Pitch Sets tab is in the colour of the pitch set the controls set, and
    // names it.
    const auto several = synthProcessor.getNumPitchSets() > 1;
    tabs.setLabelColour ((int) Tab::pitchSets, several ? std::optional (PitchSetScreen::getColour (active).brighter (0.3f)) : std::nullopt);
    tabs.setTabTooltip ((int) Tab::pitchSets, several ? "The controls set " + synthProcessor.getPitchSetName (active)
                                                      : juce::String ("Set the sound differently for ranges of keys"));

    // The keys the active pitch set plays are marked, unless it's the default with no others.
    std::bitset<128> keys;

    for (int note = 0; note < 128 && several; ++note)
        keys[(size_t) note] = synthProcessor.getPitchSetForNote (note) == active;

    keyboard.setMarkedKeys (keys, PitchSetScreen::getColour (active));
}

juce::String SynthEditor::getPitchSetText() const
{
    return synthProcessor.getNumPitchSets() > 1 ? synthProcessor.getPitchSetName (synthProcessor.getActivePitchSet())
                                                : juce::String ("Pitch Sets");
}

//==============================================================================
bool SynthEditor::keyPressed (const juce::KeyPress& key)
{
    const auto command = juce::ModifierKeys::commandModifier;

    if (key == juce::KeyPress ('z', command, 0) || key == juce::KeyPress ('z', command | juce::ModifierKeys::shiftModifier, 0))
    {
        // Whatever was just changed is a step of its own, even if it hasn't been taken as one yet.
        rememberChanges();

        if (key.getModifiers().isShiftDown() ? synthProcessor.redo() : synthProcessor.undo())
        {
            rememberedChangeCount = synthProcessor.getChangeCount();
            userTouched = false;
            refresh();
        }

        return true;
    }

    userTouched = true;
    return false;
}

void SynthEditor::mouseDown (const juce::MouseEvent&)
{
    userTouched = true;
}

void SynthEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&)
{
    userTouched = true;
    lastScrollTime = juce::Time::getMillisecondCounter();
}

void SynthEditor::rememberChanges()
{
    const auto changeCount = synthProcessor.getChangeCount();

    if (changeCount == rememberedChangeCount)
        return;

    // A drag or a scroll is one step, taken once it's finished.
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()
         || juce::Time::getMillisecondCounter() - lastScrollTime < 400)
        return;

    if (userTouched)
        synthProcessor.rememberForUndo();
    else
        synthProcessor.updateUndoStep();

    rememberedChangeCount = changeCount;
    userTouched = false;
}

void SynthEditor::timerCallback()
{
    refresh();
    showModulation();
    rememberChanges();

    // In the standalone app the computer keyboard plays the piano straight away.
    if (! focusGiven && isShowing() && synthProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        keyboard.grabKeyboardFocus();
        focusGiven = true;
    }
}
