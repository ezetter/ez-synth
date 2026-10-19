#include "ModulatableControls.h"

namespace modulatable
{
    namespace
    {
        const juce::Identifier targetProperty ("modulationTarget");
        const juce::Identifier livePositionProperty ("modulationLivePosition");
    }

    void setTarget (juce::Component& control, int target)
    {
        control.getProperties().set (targetProperty, target);
    }

    std::optional<int> getTarget (const juce::Component& control)
    {
        if (const auto* target = control.getProperties().getVarPointer (targetProperty))
            return (int) *target;

        return std::nullopt;
    }

    void setLivePosition (juce::Component& control, std::optional<float> position)
    {
        const auto current = getLivePosition (control);

        if (current.has_value() == position.has_value() && (! position.has_value() || juce::exactlyEqual (*current, *position)))
            return;

        if (position.has_value())
            control.getProperties().set (livePositionProperty, *position);
        else
            control.getProperties().remove (livePositionProperty);

        control.repaint();
    }

    std::optional<float> getLivePosition (const juce::Component& control)
    {
        if (const auto* position = control.getProperties().getVarPointer (livePositionProperty))
            return (float) *position;

        return std::nullopt;
    }

    void requestEditor (juce::Component& control)
    {
        if (auto* handler = control.findParentComponentOfClass<ModulationHandler>())
            handler->showModulationEditor (control);
    }
}

double ModulatableSlider::snapValue (double attemptedValue, DragMode dragMode)
{
    if (snap == nullptr || dragMode == notDragging || juce::ModifierKeys::currentModifiers.isAltDown())
        return attemptedValue;

    return snap (attemptedValue);
}

void ModulatableSlider::mouseDown (const juce::MouseEvent& e)
{
    alternateClick = e.mods.isPopupMenu();

    if (alternateClick)
        modulatable::requestEditor (*this);
    else
        Slider::mouseDown (e);
}

void ModulatableSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! alternateClick)
        Slider::mouseDrag (e);
}

void ModulatableSlider::mouseUp (const juce::MouseEvent& e)
{
    if (! alternateClick)
        Slider::mouseUp (e);
}

void ModulatableSlider::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        Slider::mouseDoubleClick (e);
}

void ModulatableToggle::mouseDown (const juce::MouseEvent& e)
{
    alternateClick = e.mods.isPopupMenu();

    if (alternateClick)
        modulatable::requestEditor (*this);
    else
        ToggleButton::mouseDown (e);
}

void ModulatableToggle::mouseDrag (const juce::MouseEvent& e)
{
    if (! alternateClick)
        ToggleButton::mouseDrag (e);
}

void ModulatableToggle::mouseUp (const juce::MouseEvent& e)
{
    if (! alternateClick)
        ToggleButton::mouseUp (e);
}
