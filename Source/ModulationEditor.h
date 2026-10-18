#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Sets which LFO or envelope moves a control, how far, and which way. It opens next to the control when the
    control is right-clicked or Control-clicked.
*/
class ModulationEditor final : public juce::Component
{
public:
    ModulationEditor (SynthProcessor&, int target);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showAssignment();
    void apply();
    juce::String describe() const;

    SynthProcessor& synthProcessor;
    const int target;

    ChoiceButtons sourceChoice { { "None", "LFO 1", "LFO 2", "LFO 3", "Env 1", "Env 2", "Env 3" } };
    juce::Slider range;
    ChoiceButtons polarity { { "Normal", "Inverted" } };

    juce::Rectangle<int> titleArea, sourceCaption, rangeCaption, rangeValue, polarityCaption, descriptionArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationEditor)
};
