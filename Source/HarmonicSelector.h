#pragma once

#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Two rows of buttons, 1 to 8 and 9 to 16, for how many harmonics to play the wave with, from the 1st up. Each lit
    button is coloured like its harmonic. A right-click or Control-click asks for an LFO to move the number instead.
*/
class HarmonicSelector final : public juce::Component
{
public:
    explicit HarmonicSelector (juce::RangedAudioParameter&);

    /** Switches to setting another parameter, such as another oscillator's number of harmonics. */
    void setParameter (juce::RangedAudioParameter&);

    /** Which harmonics the wave doesn't have. Their buttons are lit only in outline. */
    void setSilentHarmonics (std::array<bool, waveform::maxHarmonics>);

    /** How many buttons there are in each row, and how many rows. */
    static constexpr int perRow = 8;
    static constexpr int numRows = (waveform::maxHarmonics + perRow - 1) / perRow;
    static constexpr int preferredHeight = 60;

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
    bool alternateClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSelector)
};
