#include "HarmonicSelector.h"

#include "Modulation.h"
#include "ModulatableControls.h"
#include "SynthLookAndFeel.h"

HarmonicSelector::HarmonicSelector (juce::RangedAudioParameter& parameter)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
    setTitle ("Number of harmonics");
    setParameter (parameter);
}

void HarmonicSelector::setParameter (juce::RangedAudioParameter& parameter)
{
    attachment = std::make_unique<juce::ParameterAttachment> (parameter, [this] (float value)
    {
        selected = juce::roundToInt (value);
        repaint();
    });

    attachment->sendInitialUpdate();
}

void HarmonicSelector::setSilentHarmonics (std::array<bool, waveform::maxHarmonics> newSilent)
{
    if (newSilent != silent)
    {
        silent = newSilent;
        repaint();
    }
}

juce::Rectangle<float> HarmonicSelector::getButtonArea (int number) const
{
    constexpr auto gap = 4.0f;
    const auto width = ((float) getWidth() - gap * (perRow - 1)) / perRow;
    const auto height = ((float) getHeight() - gap * (numRows - 1)) / numRows;
    const auto row = (number - 1) / perRow, column = (number - 1) % perRow;
    return { (float) column * (width + gap), (float) row * (height + gap), width, height };
}

int HarmonicSelector::getNumberAt (juce::Point<float> position) const
{
    const auto column = juce::jlimit (0, perRow - 1, (int) (position.x / (float) juce::jmax (1, getWidth()) * perRow));
    const auto row = juce::jlimit (0, numRows - 1, (int) (position.y / (float) juce::jmax (1, getHeight()) * numRows));
    return juce::jlimit (1, waveform::maxHarmonics, 1 + row * perRow + column);
}

void HarmonicSelector::paint (juce::Graphics& g)
{
    for (int number = 1; number <= waveform::maxHarmonics; ++number)
    {
        const auto area = getButtonArea (number).reduced (0.5f);
        const auto colour = colours::forPartial (number - 1);
        const auto lit = number <= selected && ! silent[(size_t) number - 1];

        if (lit)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (area, 5.0f);
        }
        else if (number <= selected)
        {
            // Played, but the wave doesn't have this harmonic, as a square has no 2nd.
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (area, 5.0f);
            g.setColour (colour.withAlpha (0.7f));
            g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
        }
        else
        {
            g.setColour (colours::panel.brighter (0.08f));
            g.fillRoundedRectangle (area, 5.0f);
            g.setColour (colours::outline);
            g.drawRoundedRectangle (area, 5.0f, 1.0f);
        }

        if (number == hovered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRoundedRectangle (area, 5.0f);
        }

        if (number == selected)
        {
            g.setColour (colours::text);
            g.drawRoundedRectangle (area.reduced (1.0f), 4.0f, 2.0f);
        }

        // The number an LFO has moved the count to.
        if (const auto livePosition = modulatable::getLivePosition (*this);
            livePosition.has_value() && number == modulation::positionToHarmonics (*livePosition))
        {
            g.setColour (colours::modulation);
            g.drawRoundedRectangle (area.reduced (1.0f), 4.0f, 2.5f);
        }

        g.setColour (lit ? colours::background : (number <= selected ? colour : colours::dimText));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (juce::String (number), area, juce::Justification::centred);
    }
}

void HarmonicSelector::mouseDown (const juce::MouseEvent& e)
{
    alternateClick = e.mods.isPopupMenu();

    if (alternateClick)
    {
        modulatable::requestEditor (*this);
        return;
    }

    attachment->beginGesture();
    select (getNumberAt (e.position));
}

void HarmonicSelector::mouseDrag (const juce::MouseEvent& e)
{
    if (alternateClick)
        return;

    select (getNumberAt (e.position));
    setHovered (getNumberAt (e.position));
}

void HarmonicSelector::mouseUp (const juce::MouseEvent&)
{
    if (! alternateClick)
        attachment->endGesture();
}

void HarmonicSelector::mouseMove (const juce::MouseEvent& e)
{
    setHovered (getNumberAt (e.position));
}

void HarmonicSelector::mouseExit (const juce::MouseEvent&)
{
    setHovered (0);
}

void HarmonicSelector::select (int number)
{
    if (number != selected)
        attachment->setValueAsPartOfGesture ((float) number);
}

void HarmonicSelector::setHovered (int number)
{
    if (number != hovered)
    {
        hovered = number;
        repaint();
    }
}
