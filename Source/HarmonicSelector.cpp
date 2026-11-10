#include "HarmonicSelector.h"

#include "Modulation.h"
#include "SynthLookAndFeel.h"

HarmonicSelector::HarmonicSelector (juce::RangedAudioParameter& parameter)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, colours::accent);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.setWantsKeyboardFocus (false);
    slider.setTitle ("Number of harmonics");
    slider.onValueChange = [this] { repaint(); };
    slider.onLivePositionChange = [this] { repaint (numbersArea); };
    addAndMakeVisible (slider);

    setParameter (parameter, modulation::oscillatorHarmonicsTarget (0));
}

void HarmonicSelector::setParameter (juce::RangedAudioParameter& parameter, int target)
{
    attachment.reset();
    attachment = std::make_unique<juce::SliderParameterAttachment> (parameter, slider);
    slider.setDoubleClickReturnValue (true, waveform::defaultHarmonics);
    modulatable::setTarget (slider, target);
    repaint();
}

void HarmonicSelector::setSilentHarmonics (std::array<bool, waveform::maxHarmonics> newSilent)
{
    if (newSilent != silent)
    {
        silent = newSilent;
        repaint();
    }
}

void HarmonicSelector::resized()
{
    auto area = getLocalBounds();
    valueArea = area.removeFromRight (40).removeFromTop (22);
    area.removeFromRight (8);
    slider.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    numbersArea = area.withX (slider.getX()).withWidth (slider.getWidth());
}

juce::Rectangle<float> HarmonicSelector::getNumberArea (int number) const
{
    // A linear slider's thumb runs between half a thumb's width in from either end.
    const auto x = (float) slider.getX() + (float) slider.getPositionOfValue (number);
    const auto spacing = (float) slider.getWidth() / (float) waveform::maxHarmonics;
    return juce::Rectangle<float> (juce::jmin (22.0f, spacing - 2.0f), (float) numbersArea.getHeight())
        .withCentre ({ x, numbersArea.toFloat().getCentreY() });
}

void HarmonicSelector::paint (juce::Graphics& g)
{
    const auto selected = getNumber();

    g.setColour (colours::text);
    g.setFont (fonts::body (15.0f).boldened());
    g.drawText (juce::String (selected), valueArea, juce::Justification::centredRight);

    // The number an LFO, or anything else, has moved the count to, picked out in the modulation colour.
    const auto livePosition = modulatable::getLivePosition (slider);
    const auto liveNumber = livePosition.has_value() ? modulation::positionToHarmonics (*livePosition) : 0;

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));

    for (int number = 1; number <= waveform::maxHarmonics; ++number)
    {
        const auto area = getNumberArea (number);
        const auto colour = colours::forPartial (number - 1);
        const auto played = number <= selected;
        const auto lit = played && ! silent[(size_t) number - 1];

        if (lit)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (area, 3.0f);
        }
        else if (played)
        {
            // Played, but the wave doesn't have this harmonic, as a square has no 2nd.
            g.setColour (colour.withAlpha (0.7f));
            g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
        }

        if (number == liveNumber)
        {
            g.setColour (colours::modulation);
            g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 2.0f);
        }

        g.setColour (lit ? colours::background : (played ? colour : colours::dimText.withAlpha (0.7f)));
        g.drawText (juce::String (number), area, juce::Justification::centred);
    }
}
