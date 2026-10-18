#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Sets what moves a control: an LFO or envelope, and the key velocity, each with how far and which way. It opens
    next to the control when the control is right-clicked or Control-clicked.
*/
class ModulationEditor final : public juce::Component
{
public:
    ModulationEditor (SynthProcessor&, int target);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** The controls for one thing moving the control: what it is, or whether it's on, its range and its polarity. */
    struct Section
    {
        explicit Section (juce::StringArray choices);

        ChoiceButtons choice;
        juce::Slider range;
        ChoiceButtons polarity { { "Normal", "Inverted" } };

        juce::Rectangle<int> choiceCaption, rangeCaption, rangeValue, polarityCaption, descriptionArea;
    };

    void setUp (Section&);
    void show (Section&, const modulation::Assignment&);
    void layOut (Section&, juce::Rectangle<int>&);
    void paintSection (juce::Graphics&, const Section&, const juce::String& choiceCaption, bool active,
                       const juce::String& description);
    void apply();

    juce::String describeSource() const;
    juce::String describeVelocity() const;

    SynthProcessor& synthProcessor;
    const int target;

    Section source { { "None", "LFO 1", "LFO 2", "LFO 3", "Env 1", "Env 2", "Env 3" } };
    Section velocity { { "Off", "On" } };

    juce::Rectangle<int> titleArea, dividerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationEditor)
};
