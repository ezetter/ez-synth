#include "SynthLookAndFeel.h"

namespace colours
{
    juce::Colour forPartial (int index)
    {
        static const std::array palette {
            juce::Colour (0xffff6b6b),   // red
            juce::Colour (0xffffa94d),   // orange
            juce::Colour (0xffffe066),   // yellow
            juce::Colour (0xff69db7c),   // green
            juce::Colour (0xff3bc9db),   // cyan
            juce::Colour (0xff4d8df7),   // blue
            juce::Colour (0xff9775fa),   // violet
            juce::Colour (0xfff783ac),   // pink
            juce::Colour (0xffd8f5a2),   // pale lime
        };

        return palette[(size_t) index % palette.size()];
    }
}

namespace fonts
{
    juce::Font title()
    {
        return juce::Font (juce::FontOptions (22.0f, juce::Font::bold)).withExtraKerningFactor (0.16f);
    }

    juce::Font caption()
    {
        return juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.14f);
    }

    juce::Font body (float height)
    {
        return juce::FontOptions (height);
    }
}

SynthLookAndFeel::SynthLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::outline);
    setColour (juce::Label::textColourId, colours::text);
}

void SynthLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto trackWidth = juce::jmax (3.0f, radius * 0.11f);
    const auto arcRadius = radius - trackWidth / 2.0f;
    const juce::PathStrokeType trackStroke (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, trackStroke);

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, trackStroke);
    }

    const auto knobRadius = radius * 0.7f;
    const auto knob = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (knob.translated (0.0f, 2.5f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b404d), centre.x, knob.getY(),
                                             juce::Colour (0xff1b1e25), centre.x, knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff4a505e));
    g.drawEllipse (knob.reduced (0.5f), 1.0f);

    const auto pointerStart = centre.getPointOnCircumference (knobRadius * 0.4f, angle);
    const auto pointerEnd = centre.getPointOnCircumference (knobRadius * 0.82f, angle);
    g.setColour (colours::text);
    g.drawLine ({ pointerStart, pointerEnd }, juce::jmax (2.0f, radius * 0.06f));
}

void SynthLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.darker (0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (button.getToggleState() ? fill.brighter (0.25f) : colours::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

juce::Font SynthLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.45f), juce::Font::bold);
}
