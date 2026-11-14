#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

/** A box showing what's chosen, with a small downward triangle, which opens a menu of the choices when clicked. The
    choices can be grouped under headings, and some can be greyed out so they can't be chosen.
*/
class Dropdown final : public juce::Component
{
public:
    /** A choice, with a number for it, or a heading over the choices after it. */
    struct Entry
    {
        int id = 0;
        juce::String text;
        bool heading = false;
    };

    explicit Dropdown (std::vector<Entry>);

    /** Called when the user chooses something, with its number. */
    std::function<void (int)> onChange;

    /** Shows a choice as chosen, without calling onChange. */
    void setSelectedId (int id);
    int getSelectedId() const noexcept   { return selected; }

    /** Greys a choice out, so it can't be chosen, or lets it be chosen again. */
    void setItemEnabled (int id, bool);
    bool isItemEnabled (int id) const;

    /** Chooses something as the user would from the menu, if it can be chosen. */
    void choose (int id);

    /** The choices, as the menu lists them. */
    const std::vector<Entry>& getEntries() const noexcept   { return entries; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { repaint(); }

private:
    void showMenu();

    std::vector<Entry> entries;
    juce::Array<int> disabled;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Dropdown)
};
