#include "Dropdown.h"

#include "SynthLookAndFeel.h"

Dropdown::Dropdown (std::vector<Entry> choices)
    : entries (std::move (choices))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Dropdown::setSelectedId (int id)
{
    if (selected != id)
    {
        selected = id;
        repaint();
    }
}

void Dropdown::setItemEnabled (int id, bool enabled)
{
    if (enabled)
        disabled.removeFirstMatchingValue (id);
    else
        disabled.addIfNotAlreadyThere (id);
}

bool Dropdown::isItemEnabled (int id) const
{
    return ! disabled.contains (id);
}

void Dropdown::choose (int id)
{
    if (! isItemEnabled (id))
        return;

    setSelectedId (id);

    if (onChange)
        onChange (id);
}

void Dropdown::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto active = isEnabled();
    g.setColour (active && isMouseOver() ? colours::panel.brighter (0.12f) : colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    auto area = getLocalBounds().reduced (12, 0);

    // A small downward triangle says it opens a menu.
    const auto arrowArea = area.removeFromRight (12).toFloat();
    juce::Path arrow;
    arrow.addTriangle (arrowArea.getCentreX() - 4.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX() + 4.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    g.setColour (colours::dimText);
    g.fillPath (arrow);
    area.removeFromRight (6);

    juce::String text;

    for (const auto& entry : entries)
        if (! entry.heading && entry.id == selected)
            text = entry.text;

    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.setColour (colours::text);
    g.drawText (text, area, juce::Justification::centredLeft, true);
}

void Dropdown::mouseDown (const juce::MouseEvent&)
{
    showMenu();
}

void Dropdown::showMenu()
{
    juce::PopupMenu menu;

    for (const auto& entry : entries)
    {
        if (entry.heading)
        {
            menu.addSeparator();
            menu.addSectionHeader (entry.text);
        }
        else
        {
            // The menu can outlast the dropdown, such as when the window it's in closes.
            const auto id = entry.id;
            menu.addItem (entry.text, isItemEnabled (id), id == selected, [safe = juce::Component::SafePointer<Dropdown> (this), id]
            {
                if (safe != nullptr)
                    safe->choose (id);
            });
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()));
}
