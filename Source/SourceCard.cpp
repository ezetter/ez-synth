#include "SourceCard.h"

#include "Modulation.h"
#include "SynthLookAndFeel.h"
#include "SynthProcessor.h"

SourceCard::SourceCard (const juce::String& cardName, juce::AudioProcessorValueTreeState& parameters,
                        const juce::String& onId, const juce::String& dialId, const juce::String& caption)
    : name (cardName),
      dialCaption (caption),
      onAttachment (parameters, onId, onSwitch),
      dialAttachment (parameters, dialId, dial)
{
    onSwitch.setTitle (name + " on");
    onSwitch.setMouseClickGrabsKeyboardFocus (false);
    onSwitch.setWantsKeyboardFocus (false);
    onSwitch.onStateChange = [this] { repaint(); };
    addAndMakeVisible (onSwitch);

    const auto& dialParameter = *parameters.getParameter (dialId);
    dial.setTitle (name + " " + caption.toLowerCase());
    dial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    dial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    dial.setDoubleClickReturnValue (true, dialParameter.convertFrom0to1 (dialParameter.getDefaultValue()));
    dial.setMouseClickGrabsKeyboardFocus (false);
    dial.setWantsKeyboardFocus (false);
    dial.onValueChange = [this] { repaint (dialValueArea); };
    addAndMakeVisible (dial);

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
}

void SourceCard::setSelected (bool shouldBeSelected)
{
    if (shouldBeSelected != selected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void SourceCard::setWaveValues (std::vector<float> values)
{
    waveValues = std::move (values);
    repaint();
}

void SourceCard::setLines (const juce::String& first, const juce::String& second, const juce::String& firstDetail)
{
    if (first != firstLine || second != secondLine || firstDetail != firstLineDetail)
    {
        firstLine = first;
        secondLine = second;
        firstLineDetail = firstDetail;
        repaint();
    }
}

void SourceCard::resized()
{
    auto area = getLocalBounds().reduced (10, 10);

    auto dialColumn = area.removeFromRight (48);
    dialCaptionArea = dialColumn.removeFromTop (13);
    dialValueArea = dialColumn.removeFromBottom (15);
    dial.setBounds (dialColumn);
    area.removeFromRight (6);

    auto top = area.removeFromTop (20);
    onSwitch.setBounds (top.removeFromLeft (28));
    top.removeFromLeft (6);
    nameArea = top;

    area.removeFromTop (3);
    firstLineArea = area.removeFromTop (15);
    secondLineArea = area.removeFromTop (17);
    area.removeFromTop (4);
    waveArea = area;
}

void SourceCard::paint (juce::Graphics& g)
{
    const auto on = isOn();
    const auto bounds = getLocalBounds().toFloat().reduced (0.75f);

    g.setColour (selected ? colours::panel.brighter (0.08f) : colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (selected ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, 10.0f, selected ? 1.5f : 1.0f);

    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)).withExtraKerningFactor (0.04f));
    g.drawText (name, nameArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (12.5f));
    auto firstLineLeft = firstLineArea;

    if (firstLineDetail.isNotEmpty())
    {
        const auto detailWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), firstLineDetail)) + 2;
        g.setColour (colours::dimText);
        g.drawText (firstLineDetail, firstLineLeft.removeFromRight (detailWidth), juce::Justification::centredRight);
        firstLineLeft.removeFromRight (6);
    }

    g.setColour (on ? colours::text.withAlpha (0.85f) : colours::dimText);
    g.drawText (firstLine, firstLineLeft, juce::Justification::centredLeft, true);
    g.setColour (colours::dimText);
    g.drawText (secondLine, secondLineArea, juce::Justification::centredLeft, true);

    paintWave (g);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText (dialCaption, dialCaptionArea, juce::Justification::centred);
    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawText (dial.getTextFromValue (dial.getValue()), dialValueArea, juce::Justification::centred);
}

void SourceCard::paintWave (juce::Graphics& g) const
{
    const auto area = waveArea.toFloat();

    if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
        return;

    g.setColour (colours::plot);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (colours::gridLine);
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX() + 3.0f, area.getRight() - 3.0f);

    auto peak = 0.0f;

    for (const auto value : waveValues)
        peak = std::max (peak, std::abs (value));

    if (waveValues.size() < 2 || peak <= 1.0e-6f)
        return;

    // The wave is stretched to fill the height: the dial, not the picture, says how loud it is.
    const auto inner = area.reduced (5.0f, 4.0f);
    const auto last = (float) (waveValues.size() - 1);
    juce::Path curve;

    for (size_t i = 0; i < waveValues.size(); ++i)
    {
        const juce::Point<float> point (inner.getX() + inner.getWidth() * (float) i / last,
                                        inner.getCentreY() - waveValues[i] / peak * inner.getHeight() / 2.0f);

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    g.setColour (isOn() ? colours::sum : colours::dimText.withAlpha (0.6f));
    g.strokePath (curve, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SourceCard::mouseDown (const juce::MouseEvent&)
{
    if (onSelect != nullptr)
        onSelect();
}

//==============================================================================
OscillatorCard::OscillatorCard (int oscillator, juce::AudioProcessorValueTreeState& parameters)
    : SourceCard ("OSC " + juce::String (oscillator + 1), parameters, SynthProcessor::onId (oscillator),
                  SynthProcessor::levelId (oscillator), "LEVEL")
{
    modulatable::setTarget (onSwitch, modulation::oscillatorOnTarget (oscillator));
    modulatable::setTarget (dial, modulation::oscillatorLevelTarget (oscillator));
}

void OscillatorCard::setWave (const std::vector<waveform::Partial>& harmonics, const juce::String& waveName, float tune)
{
    std::vector<float> values (160, 0.0f);

    for (size_t i = 0; i < values.size(); ++i)
        for (const auto& harmonic : harmonics)
            values[i] += harmonic.valueAt ((double) i / (double) (values.size() - 1));

    const auto numHarmonics = (int) harmonics.size();
    setWaveValues (std::move (values));
    setLines (waveName, juce::String (numHarmonics) + (numHarmonics == 1 ? " harmonic" : " harmonics"),
              tuning::toWholeSemitones (tune) != 0 ? tuning::toText (tune) : juce::String());
}

//==============================================================================
LfoCard::LfoCard (int lfo, juce::AudioProcessorValueTreeState& parameters)
    : SourceCard ("LFO " + juce::String (lfo + 1), parameters, SynthProcessor::lfoOnId (lfo),
                  SynthProcessor::lfoPeriodId (lfo), "RATE"),
      modeAttachment (*parameters.getParameter (SynthProcessor::lfoModeId (lfo)),
                      [this] (float value) { showMode (juce::roundToInt (value)); })
{
    mode.setTitle ("LFO " + juce::String (lfo + 1) + " loops, or plays a number of times from each key");

    // The rate can be given to the mod wheel.
    modulatable::setTarget (dial, modulation::lfoRateTarget (lfo));
    mode.onChoose = [this] (int index) { modeAttachment.setValueAsCompleteGesture ((float) index); };
    mode.onClick = [this] (int index)
    {
        if (index != 1)
            return false;

        chooseTimes();
        return true;
    };

    addAndMakeVisible (mode);
    modeAttachment.sendInitialUpdate();
}

void LfoCard::showMode (int index)
{
    // Looping leaves the other button showing how many times it played from each key, to go back to.
    if (index != modulation::lfoLoops)
    {
        timesMode = index;
        mode.setLabel (1, modulation::getLfoModeNames()[index]);
    }

    mode.setChosen (index == modulation::lfoLoops ? 0 : 1);
}

void LfoCard::chooseTimes()
{
    const auto names = modulation::getLfoModeNames();
    const auto chosen = mode.getChosen() == 0 ? modulation::lfoLoops : timesMode;
    juce::PopupMenu menu;

    for (int index = 1; index <= modulation::lfoForever; ++index)
        menu.addItem (index, names[index], true, index == chosen);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&mode)
                                                  .withTargetScreenArea (mode.localAreaToGlobal (mode.getButtonBounds (1)))
                                                  .withMinimumWidth (mode.getButtonBounds (1).getWidth()),
                        [safeThis = juce::Component::SafePointer (this)] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->modeAttachment.setValueAsCompleteGesture ((float) result);
                        });
}

void LfoCard::setWave (const waveform::Shape& shape, const juce::String& waveName, std::optional<float> end)
{
    // Every few points is plenty for a picture this size. The last point leads back to the first, or on to the end.
    std::vector<float> values;

    for (size_t i = 0; i < shape.size(); i += 4)
        values.push_back (shape[i]);

    values.push_back (end.value_or (shape.front()));
    setWaveValues (std::move (values));
    setLines (waveName, {});
}

void LfoCard::resized()
{
    SourceCard::resized();
    mode.setBounds (getSecondLineArea().withWidth (juce::jmin (96, getSecondLineArea().getWidth())));
}
