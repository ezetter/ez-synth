#include "ModulationEditor.h"

#include "SynthLookAndFeel.h"

ModulationEditor::Section::Section (juce::StringArray choices, bool withPhase, bool withPolarity)
    : hasPhase (withPhase),
      hasPolarity (withPolarity),
      choice (std::move (choices))
{
}

ModulationEditor::ModulationEditor (SynthProcessor& processor, int targetToEdit)
    : synthProcessor (processor),
      target (targetToEdit)
{
    setUp (source);
    setUp (velocity);
    setUp (modWheel);

    show (source, synthProcessor.getAssignment (target));
    show (velocity, synthProcessor.getVelocityAssignment (target));
    show (modWheel, synthProcessor.getModWheelAssignment (target));

    // Learn waits for a MIDI controller to be moved; clicked again, it stops waiting.
    for (auto* button : { &learnButton, &forgetButton })
    {
        button->setMouseClickGrabsKeyboardFocus (false);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    learnButton.setClickingTogglesState (false);
    learnButton.onClick = [this]
    {
        if (synthProcessor.getMidiLearnTarget() == target)
            synthProcessor.stopMidiLearn();
        else
            synthProcessor.startMidiLearn (target);

        showMidiLearn();
    };

    forgetButton.onClick = [this]
    {
        synthProcessor.setMidiController (target, std::nullopt);
        showMidiLearn();
    };

    showMidiLearn();
    startTimerHz (15);
    setSize (470, 574);
}

ModulationEditor::~ModulationEditor()
{
    // Closed while waiting to learn, it stops waiting.
    if (synthProcessor.getMidiLearnTarget() == target)
        synthProcessor.stopMidiLearn();
}

void ModulationEditor::timerCallback()
{
    if (synthProcessor.getMidiLearnVersion() != shownMidiLearnVersion)
        showMidiLearn();
}

void ModulationEditor::showMidiLearn()
{
    shownMidiLearnVersion = synthProcessor.getMidiLearnVersion();
    const auto learning = synthProcessor.getMidiLearnTarget() == target;
    learnButton.setButtonText (learning ? "Cancel" : "Learn");
    learnButton.setToggleState (learning, juce::dontSendNotification);
    forgetButton.setEnabled (synthProcessor.getMidiController (target).has_value());
    forgetButton.setAlpha (forgetButton.isEnabled() ? 1.0f : 0.4f);
    repaint();
}

juce::String ModulationEditor::describeMidiLearn() const
{
    const auto control = modulation::getName (target).toLowerCase().replace ("lfo", "LFO");

    if (synthProcessor.getMidiLearnTarget() == target)
        return "Move a knob or slider on your MIDI controller to link it to the " + control + ".";

    if (synthProcessor.getMidiController (target).has_value())
        return modulation::isTrigger (target)
                   ? juce::String ("Moving it past halfway, either way, randomizes the harmonics.")
                   : "It sets the " + control + " across its whole travel, from 0 to 127.";

    return "Click Learn, then move a knob or slider on your MIDI controller, to have it set the " + control + ".";
}

void ModulationEditor::setUp (Section& section)
{
    section.choice.onChoose = [this] (int) { apply(); };
    section.polarity.onChoose = [this] (int) { apply(); };

    section.range.setSliderStyle (juce::Slider::LinearHorizontal);
    section.range.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    section.range.setRange (0.0, 1.0);
    section.range.setColour (juce::Slider::trackColourId, colours::modulation);
    section.range.setTitle ("Range");
    section.range.onValueChange = [this] { apply(); };

    // From in step with the LFO up to a degree short of a whole period behind it.
    section.phase.setSliderStyle (juce::Slider::LinearHorizontal);
    section.phase.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    section.phase.setRange (0.0, modulation::latestPhase * 360.0, 1.0);
    section.phase.setDoubleClickReturnValue (true, 0.0);
    section.phase.setColour (juce::Slider::trackColourId, colours::modulation);
    section.phase.setTitle ("Phase");
    section.phase.onValueChange = [this] { apply(); };

    for (auto* control : std::initializer_list<juce::Component*> { &section.choice, &section.range, &section.phase,
                                                                   &section.polarity })
    {
        control->setMouseClickGrabsKeyboardFocus (false);
        control->setWantsKeyboardFocus (false);
        addChildComponent (control);
    }

    // A control that's triggered, rather than moved, has a threshold in place of a range, and no polarity:
    // crossing the threshold either way triggers it.
    const auto triggered = modulation::isTrigger (target);

    if (triggered)
        section.range.setTitle ("Threshold");

    section.choice.setVisible (true);
    section.range.setVisible (true);
    section.phase.setVisible (section.hasPhase);
    section.polarity.setVisible (section.hasPolarity && ! triggered);
}

void ModulationEditor::show (Section& section, const modulation::Assignment& assignment)
{
    const auto isVelocity = &section == &velocity;
    const auto isSwitched = &section != &source;
    section.choice.setChosen (isSwitched ? (assignment.isActive() ? 1 : 0) : assignment.source + 1);

    // An LFO that would end up moving its own rate can't be chosen.
    if (! isSwitched)
        for (int lfo = 0; lfo < modulation::numLfos; ++lfo)
            section.choice.setButtonEnabled (lfo + 1, ! synthProcessor.wouldMakeLoop (target, lfo));

    // A range it hasn't been given yet is the usual one to start from. A trigger's slider sets its threshold, from
    // the midline to start with.
    const auto range = assignment.isActive() || ! isVelocity ? assignment.range : modulation::startingVelocityOnVolume.range;

    if (modulation::isTrigger (target))
    {
        section.range.setValue (assignment.threshold, juce::dontSendNotification);
        section.range.setDoubleClickReturnValue (true, modulation::Assignment().threshold);
    }
    else
    {
        section.range.setValue (range, juce::dontSendNotification);
        section.range.setDoubleClickReturnValue (true, isVelocity ? modulation::startingVelocityOnVolume.range : modulation::Assignment().range);
    }

    section.phase.setValue (assignment.phase * 360.0, juce::dontSendNotification);
    section.polarity.setChosen (assignment.inverted ? 1 : 0);

    // Range and polarity mean nothing until something is moving the control, and phase nothing unless it's an LFO.
    const auto isLfo = assignment.isActive() && ! isSwitched && ! modulation::isEnvelope (assignment.source);

    for (auto* control : std::initializer_list<juce::Component*> { &section.range, &section.phase, &section.polarity })
    {
        const auto enabled = control == &section.phase ? isLfo : assignment.isActive();
        control->setEnabled (enabled);
        control->setAlpha (enabled ? 1.0f : 0.4f);
    }

    repaint();
}

void ModulationEditor::apply()
{
    // A trigger's slider sets its threshold, and leaves its range as it was.
    const auto triggered = modulation::isTrigger (target);
    const auto withSlider = [triggered] (modulation::Assignment assignment, const Section& section, const modulation::Assignment& current)
    {
        if (triggered)
        {
            assignment.range = current.range;
            assignment.threshold = (float) section.range.getValue();
        }

        return assignment;
    };

    synthProcessor.setAssignment (target, withSlider ({ source.choice.getChosen() - 1, (float) source.range.getValue(),
                                                        source.polarity.getChosen() == 1, (float) (source.phase.getValue() / 360.0) },
                                                      source, synthProcessor.getAssignment (target)));

    synthProcessor.setVelocityAssignment (target, withSlider ({ velocity.choice.getChosen() == 1 ? modulation::velocitySource : -1,
                                                                (float) velocity.range.getValue(), false },
                                                              velocity, synthProcessor.getVelocityAssignment (target)));

    synthProcessor.setModWheelAssignment (target, withSlider ({ modWheel.choice.getChosen() == 1 ? modulation::modWheelSource : -1,
                                                                (float) modWheel.range.getValue(), modWheel.polarity.getChosen() == 1 },
                                                              modWheel, synthProcessor.getModWheelAssignment (target)));

    show (source, synthProcessor.getAssignment (target));
    show (velocity, synthProcessor.getVelocityAssignment (target));
    show (modWheel, synthProcessor.getModWheelAssignment (target));
}

juce::String ModulationEditor::describeSource() const
{
    const auto& assignment = synthProcessor.getAssignment (target);
    const auto control = modulation::getName (target).toLowerCase().replace ("lfo", "LFO");

    if (modulation::isTrigger (target))
    {
        const auto oscillator = "Osc " + juce::String (modulation::describe (target).oscillator + 1);

        if (! assignment.isActive())
            return "Choose an LFO or envelope to randomize " + oscillator + "'s harmonics each time it crosses the threshold, "
                   "going up or down: from 0%, its lowest, to 100%, its highest.";

        // At the very ends, it's reaching them that counts.
        const auto crossing = [&assignment]
        {
            if (assignment.threshold <= 0.0f)
                return juce::String ("gets down to its lowest");

            if (assignment.threshold >= 1.0f)
                return juce::String ("gets up to its highest");

            return "crosses " + modulation::thresholdToText (assignment.source, assignment.threshold);
        };

        if (modulation::isEnvelope (assignment.source))
            return modulation::getSourceName (assignment.source) + " randomizes " + oscillator + "'s harmonics each time it "
                 + crossing() + (assignment.threshold <= 0.0f ? juce::String (", as the newest note's release ends.")
                                 : assignment.threshold >= 1.0f ? juce::String (", as the newest note's attack ends.")
                                                                : juce::String (", rising after a key's pressed or falling, from the newest note."));

        return modulation::getSourceName (assignment.source) + " randomizes " + oscillator + "'s harmonics each time it "
             + crossing() + (assignment.threshold > 0.0f && assignment.threshold < 1.0f ? ", rising or falling" : "")
             + (assignment.phase > 0.0f ? ", " + modulation::phaseToText (assignment.phase) + " later in its period" : juce::String())
             + ".";
    }

    if (! assignment.isActive())
    {
        // An LFO can't move its own rate, nor the rate of an LFO that moves its own, so those are greyed out.
        const auto anyLoop = [this]
        {
            for (int lfo = 0; lfo < modulation::numLfos; ++lfo)
                if (synthProcessor.wouldMakeLoop (target, lfo))
                    return true;

            return false;
        }();

        return "Choose an LFO to swing the " + control + " back and forth as its wave rises and falls, or an envelope "
               "to move it each time a key is pressed."
             + juce::String (anyLoop ? " An LFO that would end up moving its own rate can't be chosen." : "");
    }

    const auto name = modulation::getSourceName (assignment.source);
    const auto direction = juce::String (assignment.inverted ? "down" : "up");

    if (modulation::isEnvelope (assignment.source))
        return name + " moves the " + control + " " + direction + " by as much as "
             + juce::String (juce::roundToInt (assignment.range * 100.0f)) + "% of its travel from where it's set, "
             + "as the envelope rises, each time a key is pressed. It comes back as the envelope falls.";

    const auto fromKey = synthProcessor.isLfoFromKey (assignment.source);
    const auto times = synthProcessor.getLfoTimes (assignment.source);
    const auto howOften = times == 0 ? juce::String ("from") : times == 1 ? juce::String ("once")
                        : times == 2 ? juce::String ("twice") : juce::String (times) + " times";
    auto timing = fromKey ? ", " + howOften + " each time a key is pressed" : juce::String();

    // Shifted to the right, a looping LFO moves the control behind the others it moves. One that starts with each key
    // starts late.
    if (assignment.phase > 0.0f)
    {
        const auto degrees = modulation::phaseToText (assignment.phase);
        const auto delay = assignment.phase * synthProcessor.getLfoPeriod (assignment.source);
        const auto seconds = juce::String (delay, delay < 1.0f ? 2 : 1) + " s";

        timing += fromKey ? ", starting " + seconds + " after the key, " + degrees + " into the LFO's period"
                          : ", " + degrees + " behind it, or " + seconds + " late";
    }

    return name + " swings the " + control + " up to " + juce::String (juce::roundToInt (assignment.range * 50.0f))
         + "% of its travel either side of where it's set, " + (assignment.inverted ? "falling" : "rising")
         + " as the LFO rises" + timing + ".";
}

juce::String ModulationEditor::describeVelocity() const
{
    const auto& assignment = synthProcessor.getVelocityAssignment (target);
    const auto control = modulation::getName (target).toLowerCase().replace ("lfo", "LFO");

    if (modulation::isTrigger (target))
        return assignment.isActive() ? "A note played harder than " + modulation::thresholdToText (assignment.source, assignment.threshold)
                                           + " after a softer one, or softer after a harder one, randomizes the harmonics."
                                     : juce::String ("Switch on to randomize the harmonics when a note's played on the other side of "
                                                     "the threshold from the one before.");

    if (! assignment.isActive())
        return "Switch on to have how hard each key is played move the " + control + ", as well as anything above.";

    return "The hardest notes leave the " + control + " where it's set. Softer ones move it down, by as much as "
         + juce::String (juce::roundToInt (assignment.range * 100.0f)) + "% of its travel for the softest.";
}

juce::String ModulationEditor::describeModWheel() const
{
    const auto& assignment = synthProcessor.getModWheelAssignment (target);
    const auto control = modulation::getName (target).toLowerCase().replace ("lfo", "LFO");

    if (modulation::isTrigger (target))
        return assignment.isActive() ? "Moving the mod wheel past " + modulation::thresholdToText (assignment.source, assignment.threshold)
                                           + ", either way, randomizes the harmonics."
                                     : juce::String ("Switch on to randomize the harmonics each time a MIDI controller's mod wheel "
                                                     "passes the threshold, either way.");

    if (! assignment.isActive())
        return "Switch on to have a MIDI controller's mod wheel move the " + control + ", as well as anything above.";

    if (modulation::describe (target).kind == modulation::Kind::lfoRate)
        return "At the bottom, the mod wheel leaves the " + control + " where it's set. Pushed up, it makes the LFO "
             + (assignment.inverted ? "slower" : "faster") + ", by as much as " + juce::String (juce::roundToInt (assignment.range * 100.0f))
             + "% of the rate dial's travel at the top.";

    return "At the bottom, the mod wheel leaves the " + control + " where it's set. Pushed up, it moves it "
         + (assignment.inverted ? "down" : "up") + ", by as much as " + juce::String (juce::roundToInt (assignment.range * 100.0f))
         + "% of its travel at the top.";
}

void ModulationEditor::layOut (Section& section, juce::Rectangle<int>& area)
{
    constexpr auto captionWidth = 104;

    auto row = area.removeFromTop (26);
    section.choiceCaption = row.removeFromLeft (captionWidth);
    section.choice.setBounds (&section != &source ? row.removeFromLeft (120) : row);
    area.removeFromTop (8);

    row = area.removeFromTop (22);
    section.rangeCaption = row.removeFromLeft (captionWidth);
    section.rangeValue = row.removeFromRight (40);
    section.range.setBounds (row);
    area.removeFromTop (8);

    if (section.hasPhase)
    {
        row = area.removeFromTop (22);
        section.phaseCaption = row.removeFromLeft (captionWidth);
        section.phaseValue = row.removeFromRight (40);
        section.phase.setBounds (row);
        area.removeFromTop (8);
    }

    if (section.hasPolarity)
    {
        row = area.removeFromTop (26);
        section.polarityCaption = row.removeFromLeft (captionWidth);
        section.polarity.setBounds (row.removeFromLeft (180));
        area.removeFromTop (6);
    }

    // An LFO's description can run to a third line, saying how its phase shifts it.
    section.descriptionArea = area.removeFromTop (section.hasPhase ? 50 : 34);
}

void ModulationEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 14);
    titleArea = area.removeFromTop (18);
    area.removeFromTop (10);

    layOut (source, area);
    area.removeFromTop (6);
    dividerArea = area.removeFromTop (1);
    area.removeFromTop (11);
    layOut (velocity, area);
    area.removeFromTop (6);
    secondDividerArea = area.removeFromTop (1);
    area.removeFromTop (11);
    layOut (modWheel, area);
    area.removeFromTop (6);
    thirdDividerArea = area.removeFromTop (1);
    area.removeFromTop (11);

    auto row = area.removeFromTop (26);
    midiCaptionArea = row.removeFromLeft (104);
    learnButton.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (8);
    forgetButton.setBounds (row.removeFromRight (90));
    row.removeFromRight (8);
    midiStatusArea = row;
    area.removeFromTop (8);
    midiDescriptionArea = area.removeFromTop (20);
}

void ModulationEditor::paintSection (juce::Graphics& g, const Section& section, const juce::String& choiceCaption,
                                     bool active, const juce::String& description)
{
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);

    const auto triggered = modulation::isTrigger (target);
    g.drawText (choiceCaption, section.choiceCaption, juce::Justification::centredLeft);

    g.drawText (triggered ? "THRESHOLD" : "RANGE", section.rangeCaption, juce::Justification::centredLeft);

    if (section.hasPhase)
        g.drawText ("PHASE", section.phaseCaption, juce::Justification::centredLeft);

    if (section.hasPolarity && ! triggered)
        g.drawText ("POLARITY", section.polarityCaption, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (active ? colours::text : colours::dimText);

    g.drawText (juce::String (juce::roundToInt (section.range.getValue() * 100.0)) + "%", section.rangeValue,
                juce::Justification::centredRight);

    if (section.hasPhase)
    {
        g.setColour (section.phase.isEnabled() ? colours::text : colours::dimText);
        g.drawText (modulation::phaseToText ((float) section.phase.getValue() / 360.0f), section.phaseValue,
                    juce::Justification::centredRight);
    }

    g.setColour (colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawFittedText (description, section.descriptionArea, juce::Justification::topLeft,
                      section.hasPhase ? 3 : 2);
}

void ModulationEditor::paint (juce::Graphics& g)
{
    g.setFont (fonts::caption());
    g.setColour (colours::modulation);
    g.drawText ("MODULATE  " + modulation::getName (target).toUpperCase(), titleArea, juce::Justification::centredLeft, true);

    paintSection (g, source, modulation::isTrigger (target) ? "TRIGGERED BY" : "MOVED BY", synthProcessor.getAssignment (target).isActive(),
                  describeSource());

    g.setColour (colours::outline);
    g.fillRect (dividerArea);

    paintSection (g, velocity, "VELOCITY", synthProcessor.getVelocityAssignment (target).isActive(), describeVelocity());

    g.setColour (colours::outline);
    g.fillRect (secondDividerArea);

    paintSection (g, modWheel, "MOD WHEEL", synthProcessor.getModWheelAssignment (target).isActive(), describeModWheel());

    g.setColour (colours::outline);
    g.fillRect (thirdDividerArea);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("MIDI CC", midiCaptionArea, juce::Justification::centredLeft);

    const auto controller = synthProcessor.getMidiController (target);
    const auto learning = synthProcessor.getMidiLearnTarget() == target;
    g.setFont (fonts::body (13.0f));
    g.setColour (learning ? colours::modulation : controller.has_value() ? colours::text : colours::dimText);
    g.drawText (learning ? juce::String ("Waiting...") : controller.has_value() ? "CC " + juce::String (*controller) : juce::String ("Not linked"),
                midiStatusArea, juce::Justification::centred);

    g.setColour (colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawFittedText (describeMidiLearn(), midiDescriptionArea, juce::Justification::topLeft, 1);
}
