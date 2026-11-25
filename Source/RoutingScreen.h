#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the window but for the keyboard, setting where an oscillator's sound goes: which
    filters it's sent through, a switch for each, all on to begin with, and then which effects, for now the reverb,
    off to begin with. The filters run in a chain, from the first to the last, and an oscillator that isn't sent
    through one skips it, going straight on to the next. After them, one sent through the reverb plays out through
    it, and one that isn't plays straight out. Buttons along the top choose the oscillator. Done, or Escape, goes
    back to the synth.
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

    /** The switch for whether the oscillator shown is sent through an effect, numbered from 0. */
    juce::ToggleButton& getEffectSwitch (int effect)   { return effectRows[(size_t) effect].sendSwitch; }

    /** The path the oscillator shown takes, such as "Osc 1 → Filter 1 → Reverb → Out". */
    juce::String describePath() const;

    /** Called when an effect's Settings button's clicked, to show the effects' screen. */
    std::function<void()> onShowEffects;

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
    juce::String describeEffect (int effect) const;
    void paintRow (juce::Graphics&, const Row&, const juce::String& title, const juce::String& sentText,
                   const juce::String& skipText) const;

    SynthProcessor& synthProcessor;
    ChoiceButtons oscillatorButtons;
    std::array<Row, modulation::numFilters> rows;
    std::array<Row, Patch::numEffects> effectRows;
    juce::TextButton effectSettingsButton { "Settings" };
    int oscillator = 0;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, oscillatorCaptionArea, filtersCaptionArea, effectsCaptionArea, chainArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingScreen)
};
