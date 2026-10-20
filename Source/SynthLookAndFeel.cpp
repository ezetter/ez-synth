#include "SynthLookAndFeel.h"

#include "ModulatableControls.h"

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
    setColour (juce::PopupMenu::backgroundColourId, colours::panel.brighter (0.04f));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::dimText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::background);
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

    // Where an LFO has moved the dial to: a faint arc from where it's set, and a dot.
    if (const auto livePosition = modulatable::getLivePosition (slider))
    {
        const auto liveAngle = startAngle + *livePosition * (endAngle - startAngle);
        juce::Path swing;
        swing.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, std::min (angle, liveAngle),
                             std::max (angle, liveAngle), true);
        g.setColour (colours::modulation.withAlpha (0.45f));
        g.strokePath (swing, trackStroke);
        g.setColour (colours::modulation);
        g.fillEllipse (juce::Rectangle<float> (trackWidth * 1.7f, trackWidth * 1.7f)
                           .withCentre (centre.getPointOnCircumference (arcRadius, liveAngle)));
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

void SynthLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), 4.0f);

    g.setColour (colours::outline.brighter (0.15f));
    g.fillRoundedRectangle (track, 2.0f);
    // The track fills to the thumb from its start, or from the value it's to fill from, such as its middle.
    auto fillFrom = track.getX();

    if (auto* modulatableSlider = dynamic_cast<ModulatableSlider*> (&slider);
        modulatableSlider != nullptr && modulatableSlider->fillOrigin.has_value())
        fillFrom = (float) slider.getPositionOfValue (juce::jlimit (slider.getMinimum(), slider.getMaximum(), *modulatableSlider->fillOrigin));

    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (track.withLeft (std::min (fillFrom, sliderPos)).withRight (std::max (fillFrom, sliderPos)), 2.0f);

    // Where an LFO has moved the slider to: a faint stretch from where it's set, and a dot.
    if (const auto livePosition = modulatable::getLivePosition (slider))
    {
        // A slider showing only part of its travel shows a position beyond that part at its end.
        auto proportion = *livePosition;

        if (auto* modulatableSlider = dynamic_cast<ModulatableSlider*> (&slider);
            modulatableSlider != nullptr && modulatableSlider->positionToProportion != nullptr)
            proportion = juce::jlimit (0.0f, 1.0f, modulatableSlider->positionToProportion (proportion));

        const auto liveX = bounds.getX() + proportion * bounds.getWidth();
        g.setColour (colours::modulation.withAlpha (0.45f));
        g.fillRoundedRectangle (track.withLeft (std::min (sliderPos, liveX)).withRight (std::max (sliderPos, liveX)), 2.0f);
        g.setColour (colours::modulation);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ liveX, bounds.getCentreY() }));
    }

    const auto thumb = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ sliderPos, bounds.getCentreY() });
    g.setColour (slider.isMouseOverOrDragging() ? colours::text : colours::text.darker (0.15f));
    g.fillEllipse (thumb);
}

void SynthLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path,
                                                 juce::Image& cachedShadow)
{
    if (cachedShadow.isNull())
    {
        cachedShadow = { juce::Image::ARGB, 1 + juce::roundToInt (path.getBounds().getRight()),
                         1 + juce::roundToInt (path.getBounds().getBottom()), true };
        juce::Graphics shadowGraphics (cachedShadow);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 14, { 0, 4 }).drawForPath (shadowGraphics, path);
    }

    g.drawImageAt (cachedShadow, 0, 0);
    g.setColour (colours::panel.brighter (0.04f));
    g.fillPath (path);
    g.setColour (colours::modulation.withAlpha (0.6f));
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

void SynthLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool)
{
    // A rounded track, with a knob that slides to the right when the switch is on.
    const auto on = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat();
    const auto track = bounds.withSizeKeepingCentre (bounds.getWidth() - 2.0f, juce::jmin (bounds.getHeight() - 2.0f, 20.0f));
    const auto radius = track.getHeight() / 2.0f;

    auto trackColour = on ? colours::accent : colours::outline.brighter (0.2f);

    if (shouldDrawButtonAsHighlighted)
        trackColour = trackColour.brighter (0.15f);

    g.setColour (trackColour);
    g.fillRoundedRectangle (track, radius);

    const auto knobCentre = juce::Point<float> (on ? track.getRight() - radius : track.getX() + radius, track.getCentreY());
    g.setColour (on ? colours::background : colours::text);
    g.fillEllipse (juce::Rectangle<float> (radius * 1.5f, radius * 1.5f).withCentre (knobCentre));

    // A switch an LFO is flicking is ringed, with a dot on the side the LFO has put it.
    if (const auto livePosition = modulatable::getLivePosition (button))
    {
        const auto liveOn = *livePosition >= 0.5f;
        g.setColour (colours::modulation);
        g.drawRoundedRectangle (track.expanded (1.0f), radius + 1.0f, 1.5f);
        g.fillEllipse (juce::Rectangle<float> (radius * 0.7f, radius * 0.7f)
                           .withCentre ({ liveOn ? track.getRight() - radius : track.getX() + radius, track.getCentreY() }));
    }
}
