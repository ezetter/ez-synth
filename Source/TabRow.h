#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

/** A row of tabs, one of which is chosen, each a label with a line under the chosen one, such as the window's
    Synth, Effects, Pitch Sets and Modulation.
*/
class TabRow final : public juce::Component,
                     public juce::TooltipClient
{
public:
    explicit TabRow (juce::StringArray labels);

    /** Called when the user chooses a tab, with its number from 0. */
    std::function<void (int)> onChoose;

    /** Shows a tab as chosen, without calling onChoose. */
    void setChosen (int index);
    int getChosen() const noexcept   { return chosen; }

    juce::String getLabel (int index) const   { return labels[index]; }

    /** Draws a tab's label in its own colour, such as the active pitch set's, or as the others are, if not set. */
    void setLabelColour (int index, std::optional<juce::Colour>);

    /** A tab's tooltip, shown when the mouse is over it. */
    void setTabTooltip (int index, const juce::String&);
    juce::String getTooltip() override;

    /** The width the tabs need, side by side. */
    int getPreferredWidth() const;

    /** Where a tab is, in the component. */
    juce::Rectangle<int> getTabBounds (int index) const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int getIndexAt (juce::Point<int>) const;
    int getTabWidth (int index) const;

    juce::StringArray labels, tooltips;
    std::vector<std::optional<juce::Colour>> labelColours;
    int chosen = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabRow)
};
