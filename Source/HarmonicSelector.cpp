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
    slider.onValueChange = [this] { repaint (valueArea); };
    slider.onLivePositionChange = [this] { showMovedNumber(); };
    addAndMakeVisible (slider);

    for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
    {
        boxes[(size_t) harmonic - 1] = std::make_unique<Box> (*this, harmonic);
        addAndMakeVisible (*boxes[(size_t) harmonic - 1]);
    }

    setParameter (parameter, modulation::oscillatorHarmonicsTarget (0));
    setOscillator (0);
}

void HarmonicSelector::setParameter (juce::RangedAudioParameter& parameter, int target)
{
    attachment.reset();
    attachment = std::make_unique<juce::SliderParameterAttachment> (parameter, slider);
    slider.setDoubleClickReturnValue (true, waveform::defaultHarmonics);
    modulatable::setTarget (slider, target);
    repaint();
}

void HarmonicSelector::setOscillator (int oscillator)
{
    for (auto& box : boxes)
    {
        modulatable::setTarget (*box, modulation::harmonicSwitchTarget (oscillator, box->harmonic));
        box->setTitle ("Osc " + juce::String (oscillator + 1) + " harmonic " + juce::String (box->harmonic) + " switch");
    }
}

void HarmonicSelector::setHarmonics (std::array<bool, waveform::maxHarmonics> played, std::array<bool, waveform::maxHarmonics> silent,
                                     waveform::HarmonicSwitches switches)
{
    for (auto& box : boxes)
    {
        const auto index = (size_t) box->harmonic - 1;

        if (box->played != played[index] || box->silent != silent[index] || box->switched != switches[index])
        {
            box->played = played[index];
            box->silent = silent[index];
            box->switched = switches[index];
            box->repaint();
        }
    }
}

void HarmonicSelector::showMovedNumber()
{
    const auto livePosition = modulatable::getLivePosition (slider);
    const auto movedNumber = livePosition.has_value() ? modulation::positionToHarmonics (*livePosition) : 0;

    for (auto& box : boxes)
    {
        if (const auto moved = box->harmonic == movedNumber; moved != box->moved)
        {
            box->moved = moved;
            box->repaint();
        }
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

    // Each box sits under the point on the slider that picks its number.
    const auto spacing = (float) slider.getWidth() / (float) waveform::maxHarmonics;
    const auto width = juce::jmin (22.0f, spacing - 2.0f);

    for (auto& box : boxes)
    {
        const auto x = (float) slider.getX() + (float) slider.getPositionOfValue (box->harmonic);
        box->setBounds (juce::Rectangle<float> (width, (float) numbersArea.getHeight())
                            .withCentre ({ x, numbersArea.toFloat().getCentreY() }).getSmallestIntegerContainer());
    }
}

void HarmonicSelector::paint (juce::Graphics& g)
{
    g.setColour (colours::text);
    g.setFont (fonts::body (15.0f).boldened());
    g.drawText (juce::String (getNumber()), valueArea, juce::Justification::centredRight);
}

//==============================================================================
HarmonicSelector::Box::Box (HarmonicSelector& owner, int number)
    : harmonic (number),
      selector (owner)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
    setWantsKeyboardFocus (false);
    setTooltip ("Click to switch harmonic " + juce::String (number) + " on or off. Right-click to have a modulator switch it.");
}

void HarmonicSelector::Box::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        modulatable::requestEditor (*this);
    else if (selector.onToggle != nullptr)
        selector.onToggle (harmonic);
}

void HarmonicSelector::Box::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (0.5f);
    const auto colour = colours::forPartial (harmonic - 1);

    // Fainter if the wave hasn't got the harmonic, as a square has no 2nd.
    const auto shade = colour.withAlpha (silent ? 0.45f : 1.0f);
    const auto clickedOn = switched == waveform::HarmonicSwitch::on;
    const auto clickedOff = switched == waveform::HarmonicSwitch::off;
    juce::Colour numberColour;

    if (clickedOn)
    {
        // Switched on by a click: solid in its colour.
        g.setColour (shade);
        g.fillRoundedRectangle (area, 3.0f);
        numberColour = colours::background;
    }
    else if (clickedOff)
    {
        // Switched off by a click: light grey, with a black number.
        g.setColour (colours::dimText);
        g.fillRoundedRectangle (area, 3.0f);
        numberColour = juce::Colours::black;
    }
    else if (played)
    {
        // Played as the slider has it: a border in its colour, on dark.
        g.setColour (colours::background);
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (shade);
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
        numberColour = shade;
    }
    else
    {
        // Above the slider: just its number, faint.
        numberColour = colours::dimText.withAlpha (0.7f);
    }

    if (isMouseOver())
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRoundedRectangle (area, 3.0f);
    }

    // The number something has moved the slider to.
    if (moved)
    {
        g.setColour (colours::modulation);
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 2.0f);
    }

    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.setColour (numberColour);
    g.drawText (juce::String (harmonic), area, juce::Justification::centred);
}
