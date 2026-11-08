#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** A row of joined buttons of which one is chosen, such as Loop and Once. */
class ChoiceButtons final : public juce::Component,
                            public juce::TooltipClient
{
public:
    explicit ChoiceButtons (juce::StringArray labels);

    /** Called when the user chooses a button, with its number from 0. */
    std::function<void (int)> onChoose;

    /** If set, called first when the user clicks a button, with its number. If it returns true, it's dealt with the
        click, such as by opening a menu, and the button isn't chosen.
    */
    std::function<bool (int)> onClick;

    /** If set, draws a picture on each button, in the colour set, in place of its label, which shows as the button's
        tooltip instead.
    */
    std::function<void (juce::Graphics&, juce::Rectangle<float> area, int index)> drawPicture;

    juce::String getTooltip() override;

    /** Shows a button as chosen, without calling onChoose. */
    void setChosen (int index);
    int getChosen() const noexcept   { return chosen; }

    /** Greys a button out, so it can't be chosen, or lets it be chosen again. */
    void setButtonEnabled (int index, bool);
    bool isButtonEnabled (int index) const   { return ! disabled.contains (index); }

    /** Changes a button's label, such as to show what's been chosen from its menu. */
    void setLabel (int index, const juce::String&);
    juce::String getLabel (int index) const   { return labels[index]; }

    /** Where a button is, in the component. */
    juce::Rectangle<int> getButtonBounds (int index) const   { return getButtonArea (index).toNearestInt(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getButtonArea (int index) const;
    int getIndexAt (juce::Point<float>) const;

    juce::StringArray labels;
    juce::Array<int> disabled;
    int chosen = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButtons)
};
