#include "NumberStepper.h"

#include "SynthLookAndFeel.h"

NumberStepper::Arrow::Arrow (int stepDirection)
    : Button (stepDirection < 0 ? "Fewer" : "More"),
      direction (stepDirection)
{
    setMouseClickGrabsKeyboardFocus (false);
    setWantsKeyboardFocus (false);
}

void NumberStepper::Arrow::paintButton (juce::Graphics& g, bool highlighted, bool pressed)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto size = 3.5f;
    const auto pointing = (float) direction;
    const auto tipX = centre.x + pointing * size / 2.0f;

    juce::Path chevron;
    chevron.startNewSubPath (tipX - pointing * size, centre.y - size);
    chevron.lineTo (tipX, centre.y);
    chevron.lineTo (tipX - pointing * size, centre.y + size);

    g.setColour (! isEnabled() ? colours::dimText.withAlpha (0.35f) : (highlighted || pressed ? colours::text : colours::dimText));
    g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

NumberStepper::NumberStepper (int lowest, int highest)
    : minimum (lowest),
      maximum (highest),
      value (lowest)
{
    down.onClick = [this] { step (-1); };
    up.onClick = [this] { step (1); };
    addAndMakeVisible (down);
    addAndMakeVisible (up);
    updateArrows();
}

void NumberStepper::setValue (int newValue)
{
    newValue = juce::jlimit (minimum, maximum, newValue);

    if (newValue != value)
    {
        value = newValue;
        updateArrows();
        repaint();
    }
}

void NumberStepper::step (int by)
{
    const auto before = value;
    setValue (value + by);

    if (value != before && onChange != nullptr)
        onChange (value);
}

void NumberStepper::updateArrows()
{
    down.setEnabled (isEnabled() && value > minimum);
    up.setEnabled (isEnabled() && value < maximum);
}

void NumberStepper::enablementChanged()
{
    updateArrows();
    repaint();
}

void NumberStepper::resized()
{
    auto area = getLocalBounds();
    down.setBounds (area.removeFromLeft (22));
    up.setBounds (area.removeFromRight (22));
}

void NumberStepper::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = juce::jmin (6.0f, bounds.getHeight() / 2.0f);

    g.setColour (colours::panel.brighter (0.08f));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    g.setColour (isEnabled() ? colours::text : colours::dimText.withAlpha (0.5f));
    g.setFont (juce::FontOptions (juce::jmin (13.0f, (float) getHeight() * 0.62f), juce::Font::bold));
    g.drawText (juce::String (value), getLocalBounds().reduced (22, 0), juce::Justification::centred);
}

void NumberStepper::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // A trackpad sends many small movements, so the number steps once the wheel has moved far enough.
    if (! isEnabled())
        return;

    wheelTravel += wheel.deltaY;

    if (std::abs (wheelTravel) >= 0.15f)
    {
        step (wheelTravel > 0.0f ? 1 : -1);
        wheelTravel = 0.0f;
    }
}
