#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Sets what moves a control: an LFO or envelope, with how far and which way, the key velocity, with how far, and
    the mod wheel, with how far and which way. It opens next to the control when the control is right-clicked or
    Control-clicked. At its foot, Learn links the control to the next knob or slider moved on a MIDI controller.
*/
class ModulationEditor final : public juce::Component,
                               private juce::Timer
{
public:
    ModulationEditor (SynthProcessor&, int target);
    ~ModulationEditor() override;

    /** The buttons that learn which MIDI controller sets the control, and forget it. */
    juce::TextButton learnButton { "Learn" }, forgetButton { "Forget" };

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** The controls for one thing moving the control: what it is, or whether it's on, its range and, for an LFO,
        envelope or the mod wheel, its polarity. The phase only counts for an LFO.
    */
    struct Section
    {
        Section (juce::StringArray choices, bool hasPhase, bool hasPolarity);

        const bool hasPhase, hasPolarity;

        ChoiceButtons choice;
        juce::Slider range;
        juce::Slider phase;   // in degrees
        ChoiceButtons polarity { { "Normal", "Inverted" } };

        juce::Rectangle<int> choiceCaption, rangeCaption, rangeValue, phaseCaption, phaseValue, polarityCaption,
                             descriptionArea;
    };

    void setUp (Section&);
    void show (Section&, const modulation::Assignment&);
    void layOut (Section&, juce::Rectangle<int>&);
    void paintSection (juce::Graphics&, const Section&, const juce::String& choiceCaption, bool active,
                       const juce::String& description);
    void apply();

    juce::String describeSource() const;
    juce::String describeVelocity() const;
    juce::String describeModWheel() const;
    juce::String describeMidiLearn() const;

    /** Shows whether the control's linked to a MIDI controller, or waiting to learn one. */
    void showMidiLearn();
    void timerCallback() override;

    SynthProcessor& synthProcessor;
    const int target;

    Section source { { "None", "LFO 1", "LFO 2", "LFO 3", "Env 1", "Env 2", "Env 3" }, true, true };

    /** Velocity always moves a control down for softer notes, so it has no polarity, nor any phase. */
    Section velocity { { "Off", "On" }, false, false };

    /** The mod wheel moves a control up as it's pushed up, or down if inverted. */
    Section modWheel { { "Off", "On" }, false, true };

    juce::Rectangle<int> titleArea, dividerArea, secondDividerArea, thirdDividerArea, midiCaptionArea, midiStatusArea,
                         midiDescriptionArea;
    int shownMidiLearnVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationEditor)
};
