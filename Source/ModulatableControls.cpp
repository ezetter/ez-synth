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

        if (auto* slider = dynamic_cast<ModulatableSlider*> (&control); slider != nullptr && slider->onLivePositionChange != nullptr)
            slider->onLivePositionChange();
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
    else if (goesRound)
        valueOnMouseDown = getValue();
    else
        Slider::mouseDown (e);
}

void ModulatableSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (alternateClick)
        return;

    if (! goesRound)
    {
        Slider::mouseDrag (e);
        return;
    }

    // Dragging right or up turns it on, and left or down back, as far as the drag goes, the range once for each
    // sensitivity's worth of pixels, and round again past either end.
    if (e.mouseWasDraggedSinceMouseDown())
    {
        const auto start = getMinimum(), length = getMaximum() - getMinimum();
        const auto pixels = (double) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
        auto value = snapValue (valueOnMouseDown + pixels * length / (double) getMouseDragSensitivity(), absoluteDrag);
        value -= length * std::floor ((value - start) / length);
        setValue (value, juce::sendNotificationSync);
    }
}

void ModulatableSlider::mouseUp (const juce::MouseEvent& e)
{
    if (! alternateClick && ! goesRound)
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

void ModulatableTextButton::mouseDown (const juce::MouseEvent& e)
{
    alternateClick = e.mods.isPopupMenu();

    if (alternateClick)
        modulatable::requestEditor (*this);
    else
        TextButton::mouseDown (e);
}

void ModulatableTextButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! alternateClick)
        TextButton::mouseDrag (e);
}

void ModulatableTextButton::mouseUp (const juce::MouseEvent& e)
{
    if (! alternateClick)
        TextButton::mouseUp (e);
}
