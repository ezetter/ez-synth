#pragma once

#include "ChoiceButtons.h"
#include "ModulatableControls.h"
#include "SynthProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The two filters' controls. Each has a switch, a choice of type, low pass, mid pass or high pass, and a cutoff dial,
    with a resonance dial for a low or high pass, or a width dial for a mid pass. The filters shape the sound of all
    the oscillators together, the first and then the second.
*/
class FilterPanel final : public juce::Component
{
public:
    explicit FilterPanel (juce::AudioProcessorValueTreeState&);

    /** The height the panel needs. */
    static constexpr int preferredHeight = 164;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Filter
    {
        Filter (juce::AudioProcessorValueTreeState&, int filterNumber);

        bool isOn() const   { return onSwitch.getToggleState(); }
        FilterType getType() const   { return (FilterType) type.getChosen(); }

        /** The dial beside the cutoff: the resonance, or for a mid pass, the width. */
        ModulatableSlider& getSecondDial()   { return getType() == FilterType::midPass ? width : resonance; }

        const SynthProcessor::FilterIds ids;
        const juce::String name;

        /** Called when the type changes, whether the user chose it, or a preset or the host set it. */
        std::function<void()> onTypeChange;

        ModulatableToggle onSwitch;
        ChoiceButtons type { { FilterStage::getShortName (FilterType::lowPass), FilterStage::getShortName (FilterType::midPass),
                               FilterStage::getShortName (FilterType::highPass) } };
        ModulatableSlider cutoff, resonance, width;
        juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
        juce::ParameterAttachment typeAttachment;
        juce::AudioProcessorValueTreeState::SliderAttachment cutoffAttachment, resonanceAttachment, widthAttachment;

        juce::Rectangle<int> area, cutoffCaption, cutoffValue, secondCaption, secondValue;
    };

    void setUp (Filter&, juce::AudioProcessorValueTreeState&);
    void showSettings (Filter&);
    void paintFilter (juce::Graphics&, Filter&);

    Filter first, second;
    juce::Rectangle<int> captionArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPanel)
};
