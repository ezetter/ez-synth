#include "HarmonicSliders.h"

#include "HarmonicVoice.h"
#include "SynthLookAndFeel.h"

namespace
{
    /** The parts of a harmonic's row, from left to right. */
    struct Row
    {
        juce::Rectangle<int> swatch, number, slider, amplitude, frequency;
    };

    constexpr int amplitudeWidth = 36;
    constexpr int frequencyWidth = 66;

    Row splitRow (juce::Rectangle<int> area)
    {
        Row row;
        row.swatch = area.removeFromLeft (16);
        area.removeFromLeft (6);
        row.number = area.removeFromLeft (16);
        row.frequency = area.removeFromRight (frequencyWidth);
        row.amplitude = area.removeFromRight (amplitudeWidth);
        area.removeFromRight (6);
        row.slider = area;
        return row;
    }

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

HarmonicSliders::HarmonicSliders()
{
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        const auto harmonic = (int) i + 1;

        // Most harmonics are weak, so the low end of each slider gets more of its length.
        juce::NormalisableRange<double> range (0.0, waveform::maxAmplitude);
        range.setSkewForCentre (0.35);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setNormalisableRange (range);
        slider.setScrollWheelEnabled (false);
        slider.setColour (juce::Slider::trackColourId, colours::forPartial ((int) i));
        slider.setTitle ("Harmonic " + juce::String (harmonic));
        slider.setMouseClickGrabsKeyboardFocus (false);
        slider.setWantsKeyboardFocus (false);
        slider.onValueChange = [this, harmonic, &slider]
        {
            if (onAmplitudeChange != nullptr)
                onAmplitudeChange (harmonic, (float) slider.getValue());
        };

        addChildComponent (slider);
    }
}

void HarmonicSliders::setHarmonics (std::vector<waveform::Partial> newHarmonics)
{
    const auto numberChanged = newHarmonics.size() != harmonics.size();
    harmonics = std::move (newHarmonics);

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setVisible (i < harmonics.size());

        // The slider being dragged already shows where the user has put it.
        if (i < harmonics.size() && ! slider.isMouseButtonDown())
            slider.setValue (harmonics[i].amplitude, juce::dontSendNotification);
    }

    if (numberChanged)
        resized();

    repaint();
}

void HarmonicSliders::setFundamental (std::optional<double> hertz)
{
    fundamental = hertz;
    repaint();
}

HarmonicSliders::Layout HarmonicSliders::getLayout() const
{
    constexpr auto dividerSpace = 12;
    constexpr auto headingHeight = 18;

    auto area = getLocalBounds();
    const auto numRows = 2 + juce::jmax (1, (int) harmonics.size());
    const auto rowHeight = juce::jlimit (14, 26, (area.getHeight() - dividerSpace - headingHeight) / numRows);
    const auto keyHeight = juce::jmin (20, rowHeight);

    Layout layout;
    layout.waveKey = area.removeFromTop (keyHeight);
    layout.sumKey = area.removeFromTop (keyHeight);
    area.removeFromTop (dividerSpace / 2);
    layout.divider = area.removeFromTop (1);
    area.removeFromTop (dividerSpace / 2 - 1);
    layout.headings = area.removeFromTop (headingHeight);

    for (auto& row : layout.rows)
        row = area.removeFromTop (rowHeight);

    return layout;
}

void HarmonicSliders::resized()
{
    const auto layout = getLayout();

    for (size_t i = 0; i < sliders.size(); ++i)
        sliders[i].setBounds (splitRow (layout.rows[i]).slider);
}

void HarmonicSliders::paint (juce::Graphics& g)
{
    const auto layout = getLayout();

    g.setFont (fonts::body (13.0f));

    auto key = layout.waveKey.toFloat();
    drawSwatch (g, key.removeFromLeft (22.0f), colours::drawn.withAlpha (0.45f), 8.0f);
    key.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Your wave", key, juce::Justification::centredLeft);

    key = layout.sumKey.toFloat();
    drawSwatch (g, key.removeFromLeft (22.0f), colours::sum, 2.5f);
    key.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Sum: the wave this oscillator plays", key, juce::Justification::centredLeft, true);

    g.setColour (colours::outline);
    g.fillRect (layout.divider);

    const auto headings = splitRow (layout.headings);
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("HARMONIC", layout.headings, juce::Justification::centredLeft);
    g.drawText ("AMPLITUDE", headings.amplitude.withLeft (headings.amplitude.getRight() - 80), juce::Justification::centredRight);

    if (fundamental.has_value())
        g.drawText ("PITCH", headings.frequency, juce::Justification::centredRight);

    for (size_t i = 0; i < harmonics.size(); ++i)
    {
        const auto& harmonic = harmonics[i];
        const auto row = splitRow (layout.rows[i]);
        const auto silent = harmonic.amplitude <= 0.0f;

        drawSwatch (g, row.swatch.toFloat(), colours::forPartial ((int) i).withAlpha (silent ? 0.35f : 1.0f), 2.5f);

        g.setFont (fonts::body (13.0f));
        g.setColour (silent ? colours::dimText : colours::text);
        g.drawText (juce::String (harmonic.harmonic), row.number, juce::Justification::centredLeft);
        g.drawText (juce::String (harmonic.amplitude, 2), row.amplitude, juce::Justification::centredRight);

        if (fundamental.has_value())
        {
            const auto frequency = *fundamental * harmonic.harmonic;
            const auto audible = frequency < HarmonicVoice::highestFrequency;

            g.setColour (audible ? colours::dimText : colours::dimText.withAlpha (0.6f));
            g.drawText (audible ? formatFrequency (frequency) : juce::String ("too high"), row.frequency,
                        juce::Justification::centredRight);
        }
    }
}
