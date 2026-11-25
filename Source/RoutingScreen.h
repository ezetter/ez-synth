#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the window but for the keyboard, setting where an oscillator's sound goes. For now
    that's which filters it's sent through: a switch for each, all on to begin with. The filters run in a chain, from
    the first to the last, and an oscillator that isn't sent through one skips it, going straight on to the next.
    Buttons along the top choose the oscillator. Done, or Escape, goes back to the synth.
*/
class RoutingScreen final : public juce::Component
{
public:
    explicit RoutingScreen (SynthProcessor&);

    /** Called when the screen's closed, and when another oscillator's chosen on it. */
    std::function<void()> onClose;
    std::function<void (int oscillator)> onOscillatorChosen;

    /** Shows the routing of an oscillator, numbered from 0. */
    void showOscillator (int oscillator);
    int getOscillator() const noexcept   { return oscillator; }

    /** Brings what's shown of the filters, their types, cutoffs and switches, up to date with the synth. */
    void refresh();

    /** The switch for whether the oscillator shown is sent through a filter, numbered from 0. */
    juce::ToggleButton& getSendSwitch (int filter)   { return rows[(size_t) filter].sendSwitch; }

    juce::TextButton doneButton { "Done" };

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Row
    {
        juce::ToggleButton sendSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        juce::Rectangle<int> area;
        juce::String shownSettings;
    };

    juce::String describeFilter (int filter) const;

    SynthProcessor& synthProcessor;
    ChoiceButtons oscillatorButtons;
    std::array<Row, modulation::numFilters> rows;
    int oscillator = 0;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, oscillatorCaptionArea, filtersCaptionArea, chainArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingScreen)
};
