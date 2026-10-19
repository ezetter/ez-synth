#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

/** Something that can show the editor for which LFO moves a control. The synth's window is one, so any control
    inside it can ask.
*/
struct ModulationHandler
{
    virtual ~ModulationHandler() = default;
    virtual void showModulationEditor (juce::Component& control) = 0;
};

/** Marking controls with the modulation target they set, and showing where an LFO has moved them. */
namespace modulatable
{
    void setTarget (juce::Component&, int target);
    std::optional<int> getTarget (const juce::Component&);

    /** Shows where an LFO has moved a control, from 0 to 1 along its travel, or that none is moving it. */
    void setLivePosition (juce::Component&, std::optional<float> position);
    std::optional<float> getLivePosition (const juce::Component&);

    /** Asks the window to show the modulation editor for a control, as a right-click or Control-click does. */
    void requestEditor (juce::Component&);
}

/** A slider that, on a right-click or Control-click, asks for the modulation editor rather than moving. */
class ModulatableSlider final : public juce::Slider
{
public:
    /** If set, moves where the user drags the slider to, such as onto a whole number. Holding Option while dragging
        leaves it where it's dragged.
    */
    std::function<double (double value)> snap;

    double snapValue (double attemptedValue, DragMode) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    bool alternateClick = false;
};

/** A switch that, on a right-click or Control-click, asks for the modulation editor rather than switching. */
class ModulatableToggle final : public juce::ToggleButton
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool alternateClick = false;
};
