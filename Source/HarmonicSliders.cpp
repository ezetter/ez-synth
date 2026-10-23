#include "HarmonicSliders.h"

#include "HarmonicVoice.h"
#include "Modulation.h"
#include "SynthLookAndFeel.h"

namespace
{
    /** The parts of a harmonic's row, from left to right. */
    struct Row
    {
        juce::Rectangle<int> swatch, number, tune, phase, slider, amplitude, frequency;
    };

    constexpr int amplitudeWidth = 36;
    constexpr int frequencyWidth = 66;

    Row splitRow (juce::Rectangle<int> area)
    {
        Row row;
        row.swatch = area.removeFromLeft (16);
        area.removeFromLeft (6);
        row.number = area.removeFromLeft (16);
        row.tune = area.removeFromLeft (area.getHeight() + 2);
        area.removeFromLeft (2);
        row.phase = area.removeFromLeft (area.getHeight() + 2);
        area.removeFromLeft (4);
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

    /** A phase dial's tooltip, such as "Harmonic 3's phase: 90°". */
    juce::String describePhase (int harmonic, double degrees)
    {
        const juce::String degreeSign (juce::CharPointer_UTF8 ("\xc2\xb0"));
        return "Harmonic " + juce::String (harmonic) + "'s phase: " + juce::String (juce::roundToInt (degrees)) + degreeSign
             + ". Double-click to set it to 0" + degreeSign + ".";
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
        const auto& amplitudes = modulation::amplitudeRange();
        const juce::NormalisableRange<double> range (amplitudes.start, amplitudes.end, amplitudes.interval, amplitudes.skew);

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

        // Every harmonic's phase goes round a whole turn, from the top of its dial, snapping to each quarter turn.
        auto& phaseDial = phaseDials[i];
        phaseDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        phaseDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        phaseDial.setRotaryParameters (0.0f, juce::MathConstants<float>::twoPi, true);
        phaseDial.setRange (0.0, 360.0);
        phaseDial.setDoubleClickReturnValue (true, 0.0);
        phaseDial.snap = [] (double degrees)
        {
            const auto quarter = 90.0 * std::round (degrees / 90.0);
            return std::abs (degrees - quarter) < 4.0 ? quarter : degrees;
        };
        phaseDial.setScrollWheelEnabled (false);
        phaseDial.setColour (juce::Slider::rotarySliderFillColourId, colours::forPartial ((int) i).withAlpha (0.75f));
        phaseDial.setTitle ("Harmonic " + juce::String (harmonic) + " phase");
        phaseDial.setMouseClickGrabsKeyboardFocus (false);
        phaseDial.setWantsKeyboardFocus (false);
        phaseDial.onValueChange = [this, harmonic, &phaseDial]
        {
            phaseDial.setTooltip (describePhase (harmonic, phaseDial.getValue()));

            if (onPhaseChange != nullptr)
                onPhaseChange (harmonic, (float) juce::degreesToRadians (phaseDial.getValue()));
        };

        addChildComponent (phaseDial);

        // A harmonic above the fundamental can be tuned, in tune in the middle. Turning the dial snaps to that.
        if (harmonic == 1)
            continue;

        auto& tuner = tuners[i];
        tuner.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        tuner.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        tuner.setRange (-waveform::furthestTune, waveform::furthestTune);
        tuner.setDoubleClickReturnValue (true, 0.0);
        tuner.fillOrigin = 0.0;
        tuner.snap = [] (double tune) { return std::abs (tune) < 0.015 ? 0.0 : tune; };
        tuner.setScrollWheelEnabled (false);
        tuner.setColour (juce::Slider::rotarySliderFillColourId, colours::forPartial ((int) i));
        tuner.setTitle ("Harmonic " + juce::String (harmonic) + " tune");
        tuner.setMouseClickGrabsKeyboardFocus (false);
        tuner.setWantsKeyboardFocus (false);
        tuner.onValueChange = [this, harmonic, &tuner]
        {
            tuner.setTooltip ("Harmonic " + juce::String (harmonic) + ", at " + juce::String (harmonic + tuner.getValue(), 2)
                              + " times the fundamental. Double-click to put it in tune.");

            if (onTuneChange != nullptr)
                onTuneChange (harmonic, (float) tuner.getValue());
        };

        addChildComponent (tuner);
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

        auto& phaseDial = phaseDials[i];
        phaseDial.setVisible (i < harmonics.size());

        if (i < harmonics.size() && ! phaseDial.isMouseButtonDown())
        {
            const auto degrees = juce::radiansToDegrees ((double) harmonics[i].phase);
            phaseDial.setValue (degrees - 360.0 * std::floor (degrees / 360.0), juce::dontSendNotification);
            phaseDial.setTooltip (describePhase ((int) i + 1, phaseDial.getValue()));
        }

        auto& tuner = tuners[i];
        tuner.setVisible (i > 0 && i < harmonics.size());

        if (i > 0 && i < harmonics.size() && ! tuner.isMouseButtonDown())
        {
            tuner.setValue (harmonics[i].tune, juce::dontSendNotification);
            tuner.setTooltip ("Harmonic " + juce::String ((int) i + 1) + ", at " + juce::String ((double) i + 1.0 + harmonics[i].tune, 2)
                              + " times the fundamental. Double-click to put it in tune.");
        }
    }

    if (numberChanged)
        resized();

    repaint();
}

void HarmonicSliders::setOscillator (int oscillator)
{
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        modulatable::setTarget (sliders[i], modulation::harmonicTarget (oscillator, (int) i + 1));
        modulatable::setTarget (phaseDials[i], modulation::harmonicPhaseTarget (oscillator, (int) i + 1));

        if (i > 0)
            modulatable::setTarget (tuners[i], modulation::harmonicTuneTarget (oscillator, (int) i + 1));
    }
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
    {
        const auto row = splitRow (layout.rows[i]);
        sliders[i].setBounds (row.slider);
        tuners[i].setBounds (row.tune);
        phaseDials[i].setBounds (row.phase);
    }
}

void HarmonicSliders::paint (juce::Graphics& g)
{
    const auto layout = getLayout();

    g.setFont (fonts::body (13.0f));

    auto key = layout.waveKey.toFloat();
    drawSwatch (g, key.removeFromLeft (22.0f), colours::drawn.withAlpha (0.45f), 8.0f);
    key.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Target wave", key, juce::Justification::centredLeft);

    key = layout.sumKey.toFloat();
    drawSwatch (g, key.removeFromLeft (22.0f), colours::sum, 2.5f);
    key.removeFromLeft (8.0f);
    g.setColour (colours::text);
    g.drawText ("Actual wave", key, juce::Justification::centredLeft, true);

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

        // A tuned harmonic's frequency, or its number, is off the whole number's.
        const auto multiple = (double) harmonic.harmonic + harmonic.tune;
        const auto tuned = ! juce::exactlyEqual (harmonic.tune, 0.0f);

        if (! fundamental.has_value() && tuned)
        {
            g.setColour (colours::dimText);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (multiple, 2), row.frequency, juce::Justification::centredRight);
        }

        if (fundamental.has_value())
        {
            const auto frequency = *fundamental * multiple;
            const auto audible = frequency < HarmonicVoice::highestFrequency;

            g.setColour (audible ? colours::dimText : colours::dimText.withAlpha (0.6f));
            g.drawText (audible ? formatFrequency (frequency) : juce::String ("too high"), row.frequency,
                        juce::Justification::centredRight);
        }
    }
}
