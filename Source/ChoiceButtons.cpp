#include "ChoiceButtons.h"

#include "SynthLookAndFeel.h"

ChoiceButtons::ChoiceButtons (juce::StringArray buttonLabels)
    : labels (std::move (buttonLabels))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
}

void ChoiceButtons::setChosen (int index)
{
    if (index != chosen)
    {
        chosen = index;
        repaint();
    }
}

juce::Rectangle<float> ChoiceButtons::getButtonArea (int index) const
{
    const auto width = (float) getWidth() / (float) labels.size();
    return { (float) index * width, 0.0f, width, (float) getHeight() };
}

int ChoiceButtons::getIndexAt (juce::Point<float> position) const
{
    return juce::jlimit (0, labels.size() - 1, (int) (position.x / (float) juce::jmax (1, getWidth()) * (float) labels.size()));
}

void ChoiceButtons::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = juce::jmin (6.0f, bounds.getHeight() / 2.0f);

    g.setColour (colours::panel.brighter (0.08f));
    g.fillRoundedRectangle (bounds, corner);

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto area = getButtonArea (i).reduced (1.5f);

        if (i == chosen)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (area, corner - 1.0f);
        }
        else if (i == hovered)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (area, corner - 1.0f);
        }

        g.setColour (i == chosen ? colours::background : colours::dimText);

        if (drawPicture != nullptr)
        {
            drawPicture (g, area, i);
            continue;
        }

        g.setFont (juce::FontOptions (juce::jmin (13.0f, (float) getHeight() * 0.62f), juce::Font::bold));
        g.drawText (labels[i], area, juce::Justification::centred);
    }

    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

juce::String ChoiceButtons::getTooltip()
{
    return drawPicture != nullptr && juce::isPositiveAndBelow (hovered, labels.size()) ? labels[hovered] : juce::String();
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const auto index = getIndexAt (e.position);
    setChosen (index);

    if (onChoose != nullptr)
        onChoose (index);
}

void ChoiceButtons::mouseMove (const juce::MouseEvent& e)
{
    if (const auto index = getIndexAt (e.position); index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void ChoiceButtons::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}
