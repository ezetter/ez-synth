#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The synth's colours. */
namespace colours
{
    inline const juce::Colour background { 0xff0f1116 };
    inline const juce::Colour panel { 0xff171a21 };
    inline const juce::Colour outline { 0xff272b35 };
    inline const juce::Colour plot { 0xff0b0d11 };
    inline const juce::Colour gridLine { 0xff1a1e26 };
    inline const juce::Colour gridLineStrong { 0xff2a2f3a };
    inline const juce::Colour text { 0xffe8eaf0 };
    inline const juce::Colour dimText { 0xff8b92a3 };
    inline const juce::Colour accent { 0xff24ab18 };

    /** The wave as drawn. */
    inline const juce::Colour drawn { 0xff9fb3d9 };

    /** The sum of the sine waves: the wave as played. */
    inline const juce::Colour sum { 0xffffffff };

    /** Anything to do with the LFOs moving controls. */
    inline const juce::Colour modulation { 0xff7ce3ff };

    /** Tells the harmonics apart, from the 1st up. */
    juce::Colour forPartial (int index);
}

namespace fonts
{
    juce::Font title();

    /** Small, spaced-out capitals for labelling each part of the window. */
    juce::Font caption();

    juce::Font body (float height = 13.0f);
}

/** Dark panels, rounded buttons and an arc-style knob. A control an LFO is moving shows where it's been moved to,
    in the modulation colour.
*/
class SynthLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    /** Draws horizontal sliders as a thin track, filled up to a round thumb in the slider's track colour. */
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;

    /** Draws toggle buttons as on/off switches. */
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
