#pragma once

#include "ModulatableControls.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** A slider, from 1 to 16, for how many harmonics to play the wave with, from the 1st up, with the number beside it.
    Under it, each harmonic's number, in its colour: lit up to the number played, and outlined for a harmonic
    that's played but that the wave doesn't have. A right-click or Control-click on the slider asks for an LFO,
    envelope, velocity or the mod wheel to move the number instead.
*/
class HarmonicSelector final : public juce::Component
{
public:
    explicit HarmonicSelector (juce::RangedAudioParameter&);

    /** Switches to setting another parameter, such as another oscillator's number of harmonics, and the control
        that moves it.
    */
    void setParameter (juce::RangedAudioParameter&, int target);

    /** Which harmonics the wave doesn't have. Their numbers are lit only in outline. */
    void setSilentHarmonics (std::array<bool, waveform::maxHarmonics>);

    /** How many harmonics it's set to play. */
    int getNumber() const   { return juce::roundToInt (slider.getValue()); }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 44;

private:
    /** Where a harmonic's number sits under the slider: under the point on the slider that picks it. */
    juce::Rectangle<float> getNumberArea (int number) const;

    ModulatableSlider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    std::array<bool, waveform::maxHarmonics> silent {};
    juce::Rectangle<int> valueArea, numbersArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSelector)
};
