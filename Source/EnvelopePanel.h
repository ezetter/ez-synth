#pragma once

#include "ModulatableControls.h"
#include "SynthProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** An envelope's controls: dials for its attack, decay, sustain and release, a picture of its shape, with where the
    newest note is in it, and what it moves.
*/
class EnvelopePanel final : public juce::Component
{
public:
    EnvelopePanel (SynthProcessor&, int envelope);

    /** Brings the picture, and what the envelope moves, up to date with the synth. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Dial
    {
        ModulatableSlider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String caption;
        juce::Rectangle<int> captionArea, valueArea;
    };

    juce::String describeUse() const;
    void paintShape (juce::Graphics&) const;

    SynthProcessor& synthProcessor;
    const int envelope;
    std::array<Dial, 4> dials;

    std::optional<SynthProcessor::EnvelopeDisplay> shownDisplay;
    int shownModulationVersion = -1;
    juce::String use;

    juce::Rectangle<int> captionArea, shapeArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopePanel)
};
