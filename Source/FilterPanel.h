#pragma once

#include "SynthProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The low pass and high pass filters' controls: a switch, a cutoff dial and a resonance dial for each. The filters
    shape the sound of all the oscillators together.
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
        Filter (juce::AudioProcessorValueTreeState&, const SynthProcessor::FilterIds&, const juce::String& filterName);

        bool isOn() const   { return onSwitch.getToggleState(); }

        juce::String name;
        juce::ToggleButton onSwitch;
        juce::Slider cutoff, resonance;
        juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
        juce::AudioProcessorValueTreeState::SliderAttachment cutoffAttachment, resonanceAttachment;

        juce::Rectangle<int> area, nameArea, cutoffCaption, cutoffValue, resonanceCaption, resonanceValue;
    };

    void setUp (Filter&, juce::AudioProcessorValueTreeState&, const SynthProcessor::FilterIds&);
    void showWhetherOn (Filter&);
    void paintFilter (juce::Graphics&, Filter&);

    Filter lowPass, highPass;
    juce::Rectangle<int> captionArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPanel)
};
