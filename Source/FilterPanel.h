#pragma once

#include "ChoiceButtons.h"
#include "ModulatableControls.h"
#include "SynthProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** One filter's controls: a switch, a choice of type, low pass, mid pass, high pass or notch, shown as the shape of
    what it lets through, and a cutoff dial. Beside the cutoff are a resonance dial for a low or high pass, a width
    dial for a mid pass, or Q and depth dials for a notch. The filters shape the sound of all the oscillators
    together, one after another, from the first to the last.
*/
class FilterPanel final : public juce::Component
{
public:
    FilterPanel (juce::AudioProcessorValueTreeState&, int filterNumber);

    /** The height the panel needs. */
    static constexpr int preferredHeight = 164;

    /** Draws a small picture of what a type of filter lets through, low frequencies at the left, in the colour set. */
    static void drawTypePicture (juce::Graphics&, juce::Rectangle<float> area, FilterType);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Dial
    {
        ModulatableSlider knob;
        juce::String caption;
        juce::Rectangle<int> captionArea, valueArea;
    };

    struct Filter
    {
        Filter (juce::AudioProcessorValueTreeState&, int number);

        bool isOn() const   { return onSwitch.getToggleState(); }
        FilterType getType() const   { return (FilterType) type.getChosen(); }

        /** The dials the filter's type uses, from the cutoff along. */
        std::vector<Dial*> getDials();

        const SynthProcessor::FilterIds ids;
        const juce::String name;

        /** Called when the type changes, whether the user chose it, or a preset or the host set it. */
        std::function<void()> onTypeChange;

        ModulatableToggle onSwitch;
        ChoiceButtons type;
        Dial cutoff, resonance, width, q, depth;
        juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
        juce::ParameterAttachment typeAttachment;
        juce::AudioProcessorValueTreeState::SliderAttachment cutoffAttachment, resonanceAttachment, widthAttachment,
                                                             qAttachment, depthAttachment;

        juce::Rectangle<int> area, dialArea;
    };

    void setUp (juce::AudioProcessorValueTreeState&);
    void showSettings();
    void layOutDials();
    void paintDials (juce::Graphics&);

    const int filterNumber;
    Filter filter;
    juce::Rectangle<int> captionArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPanel)
};
