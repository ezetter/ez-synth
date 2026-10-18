#include "ModulationEditor.h"

#include "SynthLookAndFeel.h"

ModulationEditor::Section::Section (juce::StringArray choices, bool withPolarity)
    : hasPolarity (withPolarity),
      choice (std::move (choices))
{
}

ModulationEditor::ModulationEditor (SynthProcessor& processor, int targetToEdit)
    : synthProcessor (processor),
      target (targetToEdit)
{
    setUp (source);
    setUp (velocity);

    show (source, synthProcessor.getAssignment (target));
    show (velocity, synthProcessor.getVelocityAssignment (target));
    setSize (470, 302);
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

    for (auto* control : std::initializer_list<juce::Component*> { &section.choice, &section.range, &section.polarity })
    {
        control->setMouseClickGrabsKeyboardFocus (false);
        control->setWantsKeyboardFocus (false);
        addChildComponent (control);
    }

    section.choice.setVisible (true);
    section.range.setVisible (true);
    section.polarity.setVisible (section.hasPolarity);
}

void ModulationEditor::show (Section& section, const modulation::Assignment& assignment)
{
    const auto isVelocity = &section == &velocity;
    section.choice.setChosen (isVelocity ? (assignment.isActive() ? 1 : 0) : assignment.source + 1);

    // A range it hasn't been given yet is the usual one to start from.
    const auto range = assignment.isActive() || ! isVelocity ? assignment.range : modulation::startingVelocityOnVolume.range;
    section.range.setValue (range, juce::dontSendNotification);
    section.range.setDoubleClickReturnValue (true, isVelocity ? modulation::startingVelocityOnVolume.range : modulation::Assignment().range);
    section.polarity.setChosen (assignment.inverted ? 1 : 0);

    // Range and polarity mean nothing until something is moving the control.
    for (auto* control : std::initializer_list<juce::Component*> { &section.range, &section.polarity })
    {
        control->setEnabled (assignment.isActive());
        control->setAlpha (assignment.isActive() ? 1.0f : 0.4f);
    }

    repaint();
}

void ModulationEditor::apply()
{
    synthProcessor.setAssignment (target, { source.choice.getChosen() - 1, (float) source.range.getValue(),
                                            source.polarity.getChosen() == 1 });

    synthProcessor.setVelocityAssignment (target, { velocity.choice.getChosen() == 1 ? modulation::velocitySource : -1,
                                                    (float) velocity.range.getValue(), false });

    show (source, synthProcessor.getAssignment (target));
    show (velocity, synthProcessor.getVelocityAssignment (target));
}

juce::String ModulationEditor::describeSource() const
{
    const auto& assignment = synthProcessor.getAssignment (target);
    const auto control = modulation::getName (target).toLowerCase();

    if (! assignment.isActive())
        return "Choose an LFO to swing the " + control + " back and forth as its wave rises and falls, or an envelope "
               "to move it each time a key is pressed.";

    const auto name = modulation::getSourceName (assignment.source);
    const auto direction = juce::String (assignment.inverted ? "down" : "up");

    if (modulation::isEnvelope (assignment.source))
        return name + " moves the " + control + " " + direction + " by as much as "
             + juce::String (juce::roundToInt (assignment.range * 100.0f)) + "% of its travel from where it's set, "
             + "as the envelope rises, each time a key is pressed. It comes back as the envelope falls.";

    const auto timing = synthProcessor.isLfoOnce (assignment.source) ? ", once each time a key is pressed" : "";

    return name + " swings the " + control + " up to " + juce::String (juce::roundToInt (assignment.range * 50.0f))
         + "% of its travel either side of where it's set, " + (assignment.inverted ? "falling" : "rising")
         + " as the LFO rises" + timing + ".";
}

juce::String ModulationEditor::describeVelocity() const
{
    const auto& assignment = synthProcessor.getVelocityAssignment (target);
    const auto control = modulation::getName (target).toLowerCase();

    if (! assignment.isActive())
        return "Switch on to have how hard each key is played move the " + control + ", as well as anything above.";

    return "The hardest notes leave the " + control + " where it's set. Softer ones move it down, by as much as "
         + juce::String (juce::roundToInt (assignment.range * 100.0f)) + "% of its travel for the softest.";
}

void ModulationEditor::layOut (Section& section, juce::Rectangle<int>& area)
{
    constexpr auto captionWidth = 84;

    auto row = area.removeFromTop (26);
    section.choiceCaption = row.removeFromLeft (captionWidth);
    section.choice.setBounds (&section == &velocity ? row.removeFromLeft (120) : row);
    area.removeFromTop (8);

    row = area.removeFromTop (22);
    section.rangeCaption = row.removeFromLeft (captionWidth);
    section.rangeValue = row.removeFromRight (40);
    section.range.setBounds (row);
    area.removeFromTop (8);

    if (section.hasPolarity)
    {
        row = area.removeFromTop (26);
        section.polarityCaption = row.removeFromLeft (captionWidth);
        section.polarity.setBounds (row.removeFromLeft (180));
        area.removeFromTop (6);
    }

    section.descriptionArea = area.removeFromTop (34);
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
}

void ModulationEditor::paintSection (juce::Graphics& g, const Section& section, const juce::String& choiceCaption,
                                     bool active, const juce::String& description)
{
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);

    g.drawText (choiceCaption, section.choiceCaption, juce::Justification::centredLeft);
    g.drawText ("RANGE", section.rangeCaption, juce::Justification::centredLeft);

    if (section.hasPolarity)
        g.drawText ("POLARITY", section.polarityCaption, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (active ? colours::text : colours::dimText);
    g.drawText (juce::String (juce::roundToInt (section.range.getValue() * 100.0)) + "%", section.rangeValue,
                juce::Justification::centredRight);

    g.setColour (colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawFittedText (description, section.descriptionArea, juce::Justification::topLeft, 2);
}

void ModulationEditor::paint (juce::Graphics& g)
{
    g.setFont (fonts::caption());
    g.setColour (colours::modulation);
    g.drawText ("MODULATE  " + modulation::getName (target).toUpperCase(), titleArea, juce::Justification::centredLeft, true);

    paintSection (g, source, "MOVED BY", synthProcessor.getAssignment (target).isActive(), describeSource());

    g.setColour (colours::outline);
    g.fillRect (dividerArea);

    paintSection (g, velocity, "VELOCITY", synthProcessor.getVelocityAssignment (target).isActive(), describeVelocity());
}
