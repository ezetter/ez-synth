#include "Legend.h"

#include "HarmonicVoice.h"
#include "SynthLookAndFeel.h"

namespace
{
    juce::String formatFrequency (double hertz)
    {
        if (hertz < 1000.0)
            return juce::String (hertz, 1) + " Hz";

        return juce::String (hertz / 1000.0, 2) + " kHz";
    }

    void drawSwatch (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float thickness)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (area.withSizeKeepingCentre (area.getWidth(), thickness), thickness / 2.0f);
    }
}

Legend::Legend()
{
    setInterceptsMouseClicks (false, false);
}

void Legend::setPartials (std::vector<waveform::Partial> newPartials, int newNumRequested)
{
    partials = std::move (newPartials);
    numRequested = newNumRequested;
    repaint();
}

void Legend::setFundamental (std::optional<double> hertz)
{
    fundamental = hertz;
    repaint();
}

void Legend::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto numRows = 3 + (int) partials.size();
    const auto rowHeight = juce::jlimit (14.0f, 22.0f, (area.getHeight() - 12.0f) / (float) numRows);
    const auto swatchWidth = 22.0f;

    g.setFont (fonts::body (13.0f));

    auto row = area.removeFromTop (rowHeight);
    drawSwatch (g, row.removeFromLeft (swatchWidth), colours::drawn.withAlpha (0.45f), 8.0f);
    row.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Your drawing", row, juce::Justification::centredLeft);

    row = area.removeFromTop (rowHeight);
    drawSwatch (g, row.removeFromLeft (swatchWidth), colours::sum, 2.5f);
    row.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Sum of the sine waves: what you hear", row, juce::Justification::centredLeft, true);

    area.removeFromTop (6.0f);
    g.setColour (colours::outline);
    g.fillRect (area.removeFromTop (1.0f));
    area.removeFromTop (5.0f);

    const auto loudest = partials.empty() ? 1.0f : partials.front().amplitude;

    for (size_t i = 0; i < partials.size(); ++i)
    {
        const auto& partial = partials[i];
        row = area.removeFromTop (rowHeight);

        drawSwatch (g, row.removeFromLeft (swatchWidth), colours::forPartial ((int) i), 2.5f);
        row.removeFromLeft (8.0f);

        auto frequencyArea = row.removeFromRight (74.0f);
        auto levelArea = row.removeFromRight (44.0f);

        g.setColour (colours::text);
        g.drawText ("Harmonic " + juce::String (partial.harmonic), row, juce::Justification::centredLeft);
        g.drawText (juce::String (juce::roundToInt (100.0f * partial.amplitude / loudest)) + "%", levelArea,
                    juce::Justification::centredRight);

        if (fundamental.has_value())
        {
            const auto frequency = *fundamental * partial.harmonic;
            const auto audible = frequency < HarmonicVoice::highestFrequency;

            g.setColour (audible ? colours::dimText : colours::dimText.withAlpha (0.6f));
            g.drawText (audible ? formatFrequency (frequency) : juce::String ("too high"), frequencyArea,
                        juce::Justification::centredRight);
        }
    }

    if ((int) partials.size() < numRequested)
    {
        const auto count = (int) partials.size();
        const auto message = count == 0 ? juce::String ("The wave is flat, so it makes no sound.")
                                        : "This wave has only " + juce::String (count) + (count == 1 ? " harmonic." : " harmonics.");

        area.removeFromTop (4.0f);
        g.setColour (colours::dimText);
        g.setFont (fonts::body (12.5f));
        g.drawFittedText (message, area.removeFromTop (rowHeight * 2.0f).toNearestInt(), juce::Justification::topLeft, 2);
    }
}
