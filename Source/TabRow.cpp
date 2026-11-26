#include "TabRow.h"

#include "SynthLookAndFeel.h"

namespace
{
    juce::Font tabFont()   { return fonts::body (15.0f).boldened(); }
    constexpr int tabPadding = 18;
}

TabRow::TabRow (juce::StringArray tabLabels)
    : labels (std::move (tabLabels))
{
    for (int i = 0; i < labels.size(); ++i)
        tooltips.add ({});

    labelColours.resize ((size_t) labels.size());
    setRepaintsOnMouseActivity (true);
}

void TabRow::setChosen (int index)
{
    if (chosen == index)
        return;

    chosen = index;
    repaint();
}

void TabRow::setLabelColour (int index, std::optional<juce::Colour> colour)
{
    if (labelColours[(size_t) index] == colour)
        return;

    labelColours[(size_t) index] = colour;
    repaint();
}

void TabRow::setTabTooltip (int index, const juce::String& tooltip)
{
    tooltips.set (index, tooltip);
}

juce::String TabRow::getTooltip()
{
    return hovered >= 0 ? tooltips[hovered] : juce::String();
}

int TabRow::getTabWidth (int index) const
{
    return juce::roundToInt (juce::GlyphArrangement::getStringWidth (tabFont(), labels[index])) + 2 * tabPadding;
}

int TabRow::getPreferredWidth() const
{
    auto width = 0;

    for (int i = 0; i < labels.size(); ++i)
        width += getTabWidth (i);

    return width;
}

juce::Rectangle<int> TabRow::getTabBounds (int index) const
{
    // The tabs share any width beyond what they need, alike.
    const auto spare = std::max (0, getWidth() - getPreferredWidth()) / std::max (1, labels.size());
    auto x = 0;

    for (int i = 0; i < index; ++i)
        x += getTabWidth (i) + spare;

    return { x, 0, getTabWidth (index) + spare, getHeight() };
}

int TabRow::getIndexAt (juce::Point<int> point) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (getTabBounds (i).contains (point))
            return i;

    return -1;
}

void TabRow::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // A faint line along the foot, which the chosen tab's line sits on.
    g.setColour (colours::outline);
    g.fillRect (bounds.withTop (bounds.getBottom() - 1.0f));

    g.setFont (tabFont());

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto area = getTabBounds (i).toFloat();
        const auto isChosen = i == chosen;

        if (isChosen || i == hovered)
        {
            g.setColour (colours::panel.withAlpha (isChosen ? 1.0f : 0.6f));
            g.fillRoundedRectangle (area.withTrimmedBottom (1.0f).reduced (2.0f, 0.0f), 6.0f);
        }

        auto colour = labelColours[(size_t) i].value_or (isChosen ? colours::text : colours::dimText);

        if (! isChosen && i == hovered && ! labelColours[(size_t) i].has_value())
            colour = colours::text.withAlpha (0.85f);

        g.setColour (colour);
        g.drawText (labels[i], area.toNearestInt(), juce::Justification::centred);

        if (isChosen)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (area.reduced (tabPadding * 0.5f, 0.0f).withTop (area.getBottom() - 3.0f), 1.5f);
        }
    }
}

void TabRow::mouseDown (const juce::MouseEvent& event)
{
    const auto index = getIndexAt (event.getPosition());

    if (index < 0)
        return;

    setChosen (index);

    if (onChoose != nullptr)
        onChoose (index);
}

void TabRow::mouseMove (const juce::MouseEvent& event)
{
    const auto index = getIndexAt (event.getPosition());

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void TabRow::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}
