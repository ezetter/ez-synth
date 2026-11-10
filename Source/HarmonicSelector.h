#pragma once

#include "ModulatableControls.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** A slider, from 1 to 16, for how many harmonics to play the wave with, from the 1st up, with the number beside it.
    Under it, a box for each harmonic, which switches it on or off: solid in its colour while it plays, and grey
    while it doesn't. A harmonic switched off stays off as the slider passes it, and one switched on above the
    slider stays on as the slider falls below it. A right-click or Control-click on the slider asks for an LFO,
    envelope, velocity or the mod wheel to move the number, and on a box, to switch the harmonic.
*/
class HarmonicSelector final : public juce::Component
{
public:
    explicit HarmonicSelector (juce::RangedAudioParameter&);

    /** Switches to setting another parameter, such as another oscillator's number of harmonics, and the control
        that moves it.
    */
    void setParameter (juce::RangedAudioParameter&, int target);

    /** Says which oscillator's harmonics the boxes switch, so a modulator can be given each. */
    void setOscillator (int oscillator);

    /** Which harmonics play, and which of those the wave doesn't have, which are shown fainter. */
    void setHarmonics (std::array<bool, waveform::maxHarmonics> played, std::array<bool, waveform::maxHarmonics> silent);

    /** Called when a box is clicked, with its harmonic, numbered from 1. */
    std::function<void (int harmonic)> onToggle;

    /** How many harmonics it's set to play. */
    int getNumber() const   { return juce::roundToInt (slider.getValue()); }

    /** Whether a harmonic's box shows it playing. */
    bool showsPlayed (int harmonic) const   { return boxes[(size_t) harmonic - 1]->played; }

    /** A harmonic's box, numbered from 1. */
    juce::Component& getBox (int harmonic)   { return *boxes[(size_t) harmonic - 1]; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 44;

private:
    /** A harmonic's box, which switches it on or off. */
    class Box final : public juce::Component,
                      public juce::SettableTooltipClient
    {
    public:
        Box (HarmonicSelector&, int harmonic);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }

        const int harmonic;
        bool played = false, silent = false, moved = false;

    private:
        HarmonicSelector& selector;
    };

    ModulatableSlider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    std::array<std::unique_ptr<Box>, waveform::maxHarmonics> boxes;
    juce::Rectangle<int> valueArea, numbersArea;

    /** Outlines the box for the number of harmonics something has moved the slider to. */
    void showMovedNumber();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSelector)
};
