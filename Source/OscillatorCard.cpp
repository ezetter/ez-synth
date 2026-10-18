#include "OscillatorCard.h"

#include "SynthLookAndFeel.h"
#include "SynthProcessor.h"

OscillatorCard::OscillatorCard (int oscillatorIndex, juce::AudioProcessorValueTreeState& parameters)
    : oscillator (oscillatorIndex),
      onAttachment (parameters, SynthProcessor::onId (oscillatorIndex), onSwitch),
      levelAttachment (parameters, SynthProcessor::levelId (oscillatorIndex), levelKnob)
{
    const auto name = "Oscillator " + juce::String (oscillator + 1);

    onSwitch.setTitle (name + " on");
    onSwitch.setMouseClickGrabsKeyboardFocus (false);
    onSwitch.setWantsKeyboardFocus (false);
    onSwitch.onStateChange = [this] { repaint(); };
    addAndMakeVisible (onSwitch);

    const auto& levelParameter = *parameters.getParameter (SynthProcessor::levelId (oscillator));
    levelKnob.setTitle (name + " level");
    levelKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    levelKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelKnob.setDoubleClickReturnValue (true, levelParameter.convertFrom0to1 (levelParameter.getDefaultValue()));
    levelKnob.setMouseClickGrabsKeyboardFocus (false);
    levelKnob.setWantsKeyboardFocus (false);
    levelKnob.onValueChange = [this] { repaint (levelTextArea); };
    addAndMakeVisible (levelKnob);

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
}

void OscillatorCard::setSelected (bool shouldBeSelected)
{
    if (shouldBeSelected != selected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void OscillatorCard::setWave (std::vector<waveform::Partial> newPartials, const juce::String& newWaveName)
{
    partials = std::move (newPartials);
    waveName = newWaveName;
    repaint();
}

void OscillatorCard::resized()
{
    auto area = getLocalBounds().reduced (14, 10);

    auto level = area.removeFromRight (96);
    levelKnob.setBounds (level.removeFromRight (56));
    levelTextArea = level.withSizeKeepingCentre (level.getWidth(), 34);
    area.removeFromRight (12);

    auto text = area.removeFromLeft (96);
    text = text.withSizeKeepingCentre (text.getWidth(), 60);
    auto top = text.removeFromTop (22);
    onSwitch.setBounds (top.removeFromLeft (36));
    top.removeFromLeft (8);
    nameArea = top;
    text.removeFromTop (5);
    waveNameArea = text.removeFromTop (16);
    harmonicsArea = text.removeFromTop (16);

    area.removeFromLeft (10);
    waveArea = area.reduced (0, 2);
}

void OscillatorCard::paint (juce::Graphics& g)
{
    const auto on = onSwitch.getToggleState();
    const auto bounds = getLocalBounds().toFloat().reduced (0.75f);

    g.setColour (selected ? colours::panel.brighter (0.08f) : colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (selected ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, 10.0f, selected ? 1.5f : 1.0f);

    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
    g.drawText ("OSC " + juce::String (oscillator + 1), nameArea, juce::Justification::centredLeft);

    const auto numHarmonics = (int) partials.size();
    g.setColour (on ? colours::text.withAlpha (0.85f) : colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawText (waveName, waveNameArea, juce::Justification::centredLeft, true);
    g.setColour (colours::dimText);
    g.drawText (juce::String (numHarmonics) + (numHarmonics == 1 ? " harmonic" : " harmonics"), harmonicsArea,
                juce::Justification::centredLeft, true);

    paintWave (g, waveArea.toFloat(), on);

    auto levelText = levelTextArea;
    g.setFont (fonts::caption());
    g.drawText ("LEVEL", levelText.removeFromTop (14), juce::Justification::centredRight);
    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (fonts::body (13.0f));
    g.drawText (juce::String (juce::roundToInt (levelKnob.getValue() * 100.0)) + "%", levelText,
                juce::Justification::centredRight);
}

void OscillatorCard::paintWave (juce::Graphics& g, juce::Rectangle<float> area, bool on) const
{
    if (area.getWidth() < 8.0f)
        return;

    g.setColour (colours::plot);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (colours::gridLine);
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX() + 4.0f, area.getRight() - 4.0f);

    // The sum of the sine waves, stretched to fill the height, as the oscillator plays it at the same peak level
    // whatever its shape.
    const auto inner = area.reduced (6.0f, 6.0f);
    const auto numSteps = juce::roundToInt (inner.getWidth() * 2.0f);
    std::vector<float> values ((size_t) numSteps + 1);
    auto peak = 0.0f;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto position = (double) i / numSteps;

        for (const auto& partial : partials)
            values[i] += partial.valueAt (position);

        peak = std::max (peak, std::abs (values[i]));
    }

    if (peak <= 1.0e-6f)
        return;

    juce::Path curve;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const juce::Point<float> point (inner.getX() + inner.getWidth() * (float) i / (float) numSteps,
                                        inner.getCentreY() - values[i] / peak * inner.getHeight() / 2.0f);

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    g.setColour (on ? colours::sum : colours::dimText.withAlpha (0.6f));
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OscillatorCard::mouseDown (const juce::MouseEvent&)
{
    if (onSelect != nullptr)
        onSelect();
}
