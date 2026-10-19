#pragma once

#include "ModulatableControls.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Tunes an oscillator up from the note, from in tune to an octave above, with the intervals from the 2nd to the 7th
    marked along it. Dragging it snaps to whole semitones, unless Option is held. Like the other controls, it can be
    right-clicked to have an LFO, envelope or velocity move it.
*/
class TuneSlider final : public juce::Component
{
public:
    TuneSlider();

    /** Shows the tune of the given oscillator, numbered from 0, and sets it. */
    void setOscillator (juce::RangedAudioParameter& tune, int oscillator);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 62;

private:
    ModulatableSlider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::Rectangle<int> captionArea, valueArea, tickArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSlider)
};
