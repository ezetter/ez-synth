#include "WaveGrid.h"

#include "SynthLookAndFeel.h"

namespace
{
    /** The grid shows at least a little beyond -1 to 1, the range that can be drawn in, as sine waves can go
        further.
    */
    constexpr float smallestValueRange = 1.35f;

    const juce::String degrees (juce::CharPointer_UTF8 ("\xc2\xb0"));
    const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));
}

WaveGrid::WaveGrid()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setMouseClickGrabsKeyboardFocus (false);
}

void WaveGrid::setShape (const waveform::Shape& newShape)
{
    shape = newShape;
    updateValueRange();
    repaint();
}

void WaveGrid::setPartials (std::vector<waveform::Partial> newPartials)
{
    partials = std::move (newPartials);
    updateValueRange();
    repaint();
}

void WaveGrid::updateValueRange()
{
    // The scale stays put while the user draws, so the line keeps up with the mouse.
    if (lastDrawn.has_value())
        return;

    // Raising harmonics can take a wave, or its sine waves, past -1 to 1, so the grid stretches to fit them.
    auto highest = 1.0f;

    for (const auto value : shape)
        highest = std::max (highest, std::abs (value));

    for (const auto& partial : partials)
        highest = std::max (highest, partial.amplitude);

    for (int i = 0; i < 512; ++i)
    {
        auto sum = 0.0f;

        for (const auto& partial : partials)
            sum += partial.valueAt (i / 512.0);

        highest = std::max (highest, std::abs (sum));
    }

    valueRange = std::max (smallestValueRange, highest * 1.12f);
}

void WaveGrid::setOscillator (int number, bool isOn)
{
    if (number != sourceNumber || isOn != on || lfoPeriod.has_value())
    {
        sourceNumber = number;
        on = isOn;
        lfoPeriod.reset();
        runsOnce = false;
        playhead.reset();
        repaint();
    }
}

void WaveGrid::setLfo (int number, bool isOn, double periodSeconds, bool once)
{
    if (number != sourceNumber || isOn != on || lfoPeriod != periodSeconds || once != runsOnce)
    {
        sourceNumber = number;
        on = isOn;
        lfoPeriod = periodSeconds;
        runsOnce = once;
        repaint();
    }
}

void WaveGrid::setPlayhead (std::optional<float> position, float value)
{
    const auto changed = position.has_value() != playhead.has_value()
                      || (position.has_value() && ! juce::approximatelyEqual (*position, *playhead));

    if (changed)
    {
        playhead = position;
        playheadValue = value;
        repaint();
    }
}

juce::Rectangle<float> WaveGrid::getPlotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (46.0f).withTrimmedRight (20.0f)
                                     .withTrimmedTop (40.0f).withTrimmedBottom (32.0f);
}

float WaveGrid::toX (double position) const
{
    const auto plot = getPlotArea();
    return plot.getX() + (float) position * plot.getWidth();
}

float WaveGrid::toY (float value) const
{
    const auto plot = getPlotArea();
    return plot.getCentreY() - value / valueRange * plot.getHeight() / 2.0f;
}

double WaveGrid::toPosition (float x) const
{
    const auto plot = getPlotArea();
    return (double) (x - plot.getX()) / plot.getWidth();
}

float WaveGrid::toValue (float y) const
{
    const auto plot = getPlotArea();
    return (plot.getCentreY() - y) / (plot.getHeight() / 2.0f) * valueRange;
}

void WaveGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::plot);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    const auto plot = getPlotArea();
    paintGrid (g, plot);

    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plot.toNearestInt());

        // The drawn wave is a broad, faint band, so you can see how closely the sum of the sine waves follows it.
        // An oscillator that's off is drawn more faintly still.
        const auto opacity = on ? 1.0f : 0.4f;
        const auto drawnCurve = makeShapeCurve();
        g.setColour (colours::drawn.withAlpha (0.2f * opacity));
        g.strokePath (drawnCurve, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::drawn.withAlpha (0.55f * opacity));
        g.strokePath (drawnCurve, juce::PathStrokeType (1.2f));

        // An LFO plays its wave just as it's drawn, with a line showing where it's got to.
        if (lfoPeriod.has_value())
        {
            g.setColour (colours::sum.withAlpha (opacity));
            g.strokePath (drawnCurve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (playhead.has_value())
            {
                const auto x = toX (*playhead);
                const auto y = toY (playheadValue);
                g.setColour (colours::modulation.withAlpha (0.8f * opacity));
                g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
                g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));
            }
        }
        else
        {
            paintHarmonics (g, opacity);
        }
    }

    paintHeader (g, bounds);
}

void WaveGrid::paintHarmonics (juce::Graphics& g, float opacity) const
{
    // The faintest sine waves are drawn last, so the strong ones can't hide them.
    for (auto i = (int) partials.size() - 1; i >= 0; --i)
    {
        const auto& partial = partials[(size_t) i];
        g.setColour (colours::forPartial (i).withAlpha (0.9f * opacity));
        g.strokePath (makeCurve ([&partial] (double position) { return partial.valueAt (position); }),
                      juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto sumCurve = makeCurve ([this] (double position)
    {
        auto value = 0.0f;

        for (const auto& partial : partials)
            value += partial.valueAt (position);

        return value;
    });

    g.setColour (colours::sum.withAlpha (0.1f * opacity));
    g.strokePath (sumCurve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::sum.withAlpha (opacity));
    g.strokePath (sumCurve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WaveGrid::paintHeader (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto header = bounds.reduced (16.0f, 0.0f).removeFromTop (34.0f);
    const auto name = (lfoPeriod.has_value() ? "LFO " : "OSC ") + juce::String (sourceNumber);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText (name + "  " + dot + "  " + (lfoPeriod.has_value() ? "ONE PERIOD" : "ONE WAVELENGTH"), header,
                juce::Justification::centredLeft);
    g.setFont (fonts::body (12.5f));

    if (on)
    {
        g.drawText ("Click and drag to draw a wave", header, juce::Justification::centredRight);
    }
    else
    {
        g.setColour (colours::accent);
        g.drawText (name.toLowerCase().replace ("osc", "Osc").replace ("lfo", "LFO")
                        + " is off. Switch it on above " + (lfoPeriod.has_value() ? "for it to move anything." : "to hear it."),
                    header, juce::Justification::centredRight);
    }
}

void WaveGrid::paintGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    // Beyond -1 and 1 can't be drawn in, but the sine waves can go there.
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillRect (plot.withBottom (toY (1.0f)));
    g.fillRect (plot.withTop (toY (-1.0f)));

    for (int i = 0; i <= 16; ++i)
    {
        g.setColour (i % 4 == 0 ? colours::gridLineStrong : colours::gridLine);
        g.drawVerticalLine (juce::roundToInt (toX (i / 16.0)), plot.getY(), plot.getBottom());
    }

    // Lines every quarter, or further apart when the grid stretches, with -1 and 1 always marked.
    auto step = 0.25f;

    while (valueRange / step > 6.0f)
        step *= 2.0f;

    const auto numSteps = (int) (valueRange / step);

    for (int i = -numSteps; i <= numSteps; ++i)
    {
        g.setColour (i == 0 ? colours::gridLineStrong.brighter (0.4f) : colours::gridLine);
        g.drawHorizontalLine (juce::roundToInt (toY ((float) i * step)), plot.getX(), plot.getRight());
    }

    g.setColour (colours::gridLineStrong);
    g.drawHorizontalLine (juce::roundToInt (toY (1.0f)), plot.getX(), plot.getRight());
    g.drawHorizontalLine (juce::roundToInt (toY (-1.0f)), plot.getX(), plot.getRight());

    g.setFont (fonts::body (11.5f));
    g.setColour (colours::dimText);

    const auto labelStep = std::max (1, (int) (step * 2.0f));

    for (int value = -(int) valueRange; value <= (int) valueRange; ++value)
    {
        if (value % labelStep != 0 && std::abs (value) != 1)
            continue;

        const auto label = value > 0 ? "+" + juce::String (value) : (value < 0 ? minus + juce::String (-value) : juce::String ("0"));
        g.drawText (label, juce::Rectangle<float> (plot.getX() - 42.0f, toY ((float) value) - 8.0f, 34.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    // An oscillator's wavelength is marked in degrees, and an LFO's period in time.
    const auto formatTime = [] (double seconds)
    {
        if (seconds <= 0.0)
            return juce::String ("0");

        return seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms" : juce::String (seconds, 2) + " s";
    };

    for (int i = 0; i <= 4; ++i)
        g.drawText (lfoPeriod.has_value() ? formatTime (*lfoPeriod * i / 4.0) : juce::String (i * 90) + degrees,
                    juce::Rectangle<float> (48.0f, 16.0f).withCentre ({ toX (i / 4.0), plot.getBottom() + 15.0f }),
                    juce::Justification::centred);
}

juce::Path WaveGrid::makeCurve (const std::function<float (double position)>& valueAt) const
{
    // Two points per pixel keep even the highest harmonics smooth.
    const auto numSteps = juce::jmax (64, juce::roundToInt (getPlotArea().getWidth() * 2.0f));
    juce::Path curve;

    for (int i = 0; i <= numSteps; ++i)
    {
        const auto position = (double) i / numSteps;
        const juce::Point<float> point (toX (position), toY (valueAt (position)));

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    return curve;
}

juce::Path WaveGrid::makeShapeCurve() const
{
    juce::Path curve;
    curve.startNewSubPath (toX (0.0), toY (shape[0]));

    for (int i = 1; i < waveform::numPoints; ++i)
        curve.lineTo (toX ((double) i / waveform::numPoints), toY (shape[(size_t) i]));

    // A wave that repeats ends where it began. An LFO that runs once holds its last value instead.
    curve.lineTo (toX (1.0), toY (runsOnce ? shape.back() : shape[0]));
    return curve;
}

void WaveGrid::mouseDown (const juce::MouseEvent& e)
{
    lastDrawn.reset();
    drawTo (e.position);
}

void WaveGrid::mouseDrag (const juce::MouseEvent& e)
{
    drawTo (e.position);
}

void WaveGrid::mouseUp (const juce::MouseEvent&)
{
    lastDrawn.reset();
    updateValueRange();
    repaint();
}

void WaveGrid::drawTo (juce::Point<float> position)
{
    const auto point = juce::jlimit (0.0, (double) waveform::numPoints - 1.0, toPosition (position.x) * waveform::numPoints);
    const auto value = juce::jlimit (-1.0f, 1.0f, toValue (position.y));
    const auto [fromPoint, fromValue] = lastDrawn.value_or (std::pair (point, value));

    // Every point between where the mouse was and where it is now is set along the line between the two, so a
    // quick stroke leaves no gaps.
    const auto first = (int) std::ceil (std::min (fromPoint, point));
    const auto last = (int) std::floor (std::max (fromPoint, point));

    if (first > last)
    {
        shape[(size_t) juce::roundToInt (point)] = value;
    }
    else
    {
        for (int i = first; i <= last; ++i)
        {
            const auto proportion = juce::approximatelyEqual (point, fromPoint) ? 1.0 : (i - fromPoint) / (point - fromPoint);
            shape[(size_t) i] = fromValue + (float) proportion * (value - fromValue);
        }
    }

    lastDrawn = std::pair (point, value);
    repaint();

    if (onDraw != nullptr)
        onDraw (shape);
}
