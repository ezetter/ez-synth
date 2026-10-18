#include "WaveGrid.h"

#include "SynthLookAndFeel.h"

namespace
{
    /** The grid shows a little beyond -1 to 1, the range that can be drawn in, as sine waves can go further. */
    constexpr float valueRange = 1.35f;

    const juce::String degrees (juce::CharPointer_UTF8 ("\xc2\xb0"));
    const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
}

WaveGrid::WaveGrid()
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setMouseClickGrabsKeyboardFocus (false);
}

void WaveGrid::setShape (const waveform::Shape& newShape)
{
    shape = newShape;
    repaint();
}

void WaveGrid::setPartials (std::vector<waveform::Partial> newPartials)
{
    partials = std::move (newPartials);
    repaint();
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
        const auto drawnCurve = makeShapeCurve();
        g.setColour (colours::drawn.withAlpha (0.2f));
        g.strokePath (drawnCurve, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::drawn.withAlpha (0.55f));
        g.strokePath (drawnCurve, juce::PathStrokeType (1.2f));

        // The faintest sine waves are drawn last, so the strong ones can't hide them.
        for (auto i = (int) partials.size() - 1; i >= 0; --i)
        {
            const auto& partial = partials[(size_t) i];
            g.setColour (colours::forPartial (i).withAlpha (0.9f));
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

        g.setColour (colours::sum.withAlpha (0.1f));
        g.strokePath (sumCurve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::sum);
        g.strokePath (sumCurve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto header = bounds.reduced (16.0f, 0.0f).removeFromTop (34.0f);
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("ONE WAVELENGTH", header, juce::Justification::centredLeft);
    g.setFont (fonts::body (12.5f));
    g.drawText ("Click and drag to draw a wave", header, juce::Justification::centredRight);
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

    for (int i = -4; i <= 4; ++i)
    {
        g.setColour (i == 0 ? colours::gridLineStrong.brighter (0.4f)
                            : (i % 4 == 0 ? colours::gridLineStrong : colours::gridLine));
        g.drawHorizontalLine (juce::roundToInt (toY ((float) i / 4.0f)), plot.getX(), plot.getRight());
    }

    g.setFont (fonts::body (11.5f));
    g.setColour (colours::dimText);

    for (const auto& [value, label] : { std::pair (1.0f, juce::String ("+1")),
                                        std::pair (0.0f, juce::String ("0")),
                                        std::pair (-1.0f, minus + "1") })
        g.drawText (label, juce::Rectangle<float> (plot.getX() - 42.0f, toY (value) - 8.0f, 34.0f, 16.0f),
                    juce::Justification::centredRight);

    for (int i = 0; i <= 4; ++i)
        g.drawText (juce::String (i * 90) + degrees,
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

    // The wave repeats, so it ends where it began.
    curve.lineTo (toX (1.0), toY (shape[0]));
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
