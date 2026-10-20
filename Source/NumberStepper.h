#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** A whole number between two limits, with an arrow either side to step it down or up. The mouse wheel steps it too. */
class NumberStepper final : public juce::Component
{
public:
    NumberStepper (int minimum, int maximum);

    /** Called when the user steps the number, with the new number. */
    std::function<void (int)> onChange;

    /** Shows a number, without calling onChange. */
    void setValue (int);
    int getValue() const noexcept   { return value; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void enablementChanged() override;

private:
    class Arrow final : public juce::Button
    {
    public:
        explicit Arrow (int direction);
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;

    private:
        const int direction;
    };

    void step (int by);
    void updateArrows();

    const int minimum, maximum;
    int value;
    float wheelTravel = 0.0f;   // how far the wheel has moved since it last stepped the number
    Arrow down { -1 }, up { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NumberStepper)
};
