#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** A row of buttons, 1 to 9, for how many sine waves to play the wave with. Each lit button is coloured like
    the sine wave it adds.
*/
class HarmonicSelector final : public juce::Component
{
public:
    explicit HarmonicSelector (juce::RangedAudioParameter&);

    /** Switches to setting another parameter, such as another oscillator's number of harmonics. */
    void setParameter (juce::RangedAudioParameter&);

    /** How many harmonics the wave has. Buttons asking for more than that are lit only in outline. */
    void setNumAvailable (int);

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
    int numAvailable = 0;
    int hovered = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSelector)
};
