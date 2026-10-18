#pragma once

#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** A row of buttons, 1 to 9, for how many harmonics to play the wave with, from the 1st up. Each lit button is
    coloured like its harmonic.
*/
class HarmonicSelector final : public juce::Component
{
public:
    explicit HarmonicSelector (juce::RangedAudioParameter&);

    /** Switches to setting another parameter, such as another oscillator's number of harmonics. */
    void setParameter (juce::RangedAudioParameter&);

    /** Which harmonics the wave doesn't have. Their buttons are lit only in outline. */
    void setSilentHarmonics (std::array<bool, waveform::maxHarmonics>);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getButtonArea (int number) const;
    int getNumberAt (juce::Point<float>) const;
    void select (int number);
    void setHovered (int number);

    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 1;
    std::array<bool, waveform::maxHarmonics> silent {};
    int hovered = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSelector)
};
