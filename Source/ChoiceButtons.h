#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** A row of joined buttons of which one is chosen, such as Loop and Once. */
class ChoiceButtons final : public juce::Component
{
public:
    explicit ChoiceButtons (juce::StringArray labels);

    /** Called when the user chooses a button, with its number from 0. */
    std::function<void (int)> onChoose;

    /** Shows a button as chosen, without calling onChoose. */
    void setChosen (int index);
    int getChosen() const noexcept   { return chosen; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getButtonArea (int index) const;
    int getIndexAt (juce::Point<float>) const;

    juce::StringArray labels;
    int chosen = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButtons)
};
