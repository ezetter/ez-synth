#include "ModulationEditor.h"

#include "SynthLookAndFeel.h"

ModulationEditor::ModulationEditor (SynthProcessor& processor, int targetToEdit)
    : synthProcessor (processor),
      target (targetToEdit)
{
    lfoChoice.onChoose = [this] (int) { apply(); };
    polarity.onChoose = [this] (int) { apply(); };

    range.setSliderStyle (juce::Slider::LinearHorizontal);
    range.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    range.setRange (0.0, 1.0);
    range.setDoubleClickReturnValue (true, modulation::Assignment().range);
    range.setColour (juce::Slider::trackColourId, colours::modulation);
    range.setTitle ("Range");
    range.onValueChange = [this] { apply(); };

    for (auto* control : std::initializer_list<juce::Component*> { &lfoChoice, &range, &polarity })
    {
        control->setMouseClickGrabsKeyboardFocus (false);
        control->setWantsKeyboardFocus (false);
        addAndMakeVisible (control);
    }

    showAssignment();
    setSize (360, 196);
}

void ModulationEditor::showAssignment()
{
    const auto& assignment = synthProcessor.getAssignment (target);
    lfoChoice.setChosen (assignment.lfo + 1);
    range.setValue (assignment.range, juce::dontSendNotification);
    polarity.setChosen (assignment.inverted ? 1 : 0);

    // Range and polarity mean nothing until there's an LFO.
    range.setEnabled (assignment.isActive());
    polarity.setEnabled (assignment.isActive());
    polarity.setAlpha (assignment.isActive() ? 1.0f : 0.4f);
    range.setAlpha (assignment.isActive() ? 1.0f : 0.4f);
    repaint();
}

void ModulationEditor::apply()
{
    modulation::Assignment assignment;
    assignment.lfo = lfoChoice.getChosen() - 1;
    assignment.range = (float) range.getValue();
    assignment.inverted = polarity.getChosen() == 1;
    synthProcessor.setAssignment (target, assignment);
    showAssignment();
}

juce::String ModulationEditor::describe() const
{
    const auto& assignment = synthProcessor.getAssignment (target);
    const auto control = modulation::getName (target).toLowerCase();

    if (! assignment.isActive())
        return "Choose an LFO to move the " + control + " as the LFO's wave rises and falls.";

    const auto lfo = "LFO " + juce::String (assignment.lfo + 1);
    const auto swing = juce::String (juce::roundToInt (assignment.range * 50.0f)) + "%";
    const auto timing = synthProcessor.isLfoOnce (assignment.lfo) ? ", once each time a key is pressed" : "";

    return lfo + " swings the " + control + " up to " + swing + " of its travel either side of where it's set, "
         + (assignment.inverted ? "falling" : "rising") + " as the LFO rises" + timing + ".";
}

void ModulationEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 14);
    titleArea = area.removeFromTop (18);
    area.removeFromTop (10);

    constexpr auto captionWidth = 76;

    auto row = area.removeFromTop (26);
    lfoCaption = row.removeFromLeft (captionWidth);
    lfoChoice.setBounds (row);
    area.removeFromTop (10);

    row = area.removeFromTop (22);
    rangeCaption = row.removeFromLeft (captionWidth);
    rangeValue = row.removeFromRight (40);
    range.setBounds (row);
    area.removeFromTop (10);

    row = area.removeFromTop (26);
    polarityCaption = row.removeFromLeft (captionWidth);
    polarity.setBounds (row.removeFromLeft (180));
    area.removeFromTop (10);

    descriptionArea = area;
}

void ModulationEditor::paint (juce::Graphics& g)
{
    g.setFont (fonts::caption());
    g.setColour (colours::modulation);
    g.drawText ("MODULATE  " + modulation::getName (target).toUpperCase(), titleArea, juce::Justification::centredLeft, true);

    g.setColour (colours::dimText);

    for (const auto& [caption, area] : { std::pair ("LFO", lfoCaption), std::pair ("RANGE", rangeCaption),
                                         std::pair ("POLARITY", polarityCaption) })
        g.drawText (caption, area, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (synthProcessor.getAssignment (target).isActive() ? colours::text : colours::dimText);
    g.drawText (juce::String (juce::roundToInt (range.getValue() * 100.0)) + "%", rangeValue, juce::Justification::centredRight);

    g.setColour (colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawFittedText (describe(), descriptionArea, juce::Justification::topLeft, 3);
}
