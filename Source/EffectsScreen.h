#pragma once

#include "ChoiceButtons.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <deque>

/** A screen of its own, over the window but for the keyboard, setting the effects oscillators can be sent through,
    in the order they run: the delay, its time, in milliseconds or synced to the tempo, its feedback and mix, and
    then the reverb, its pre-delay, the room's size, how long it rings, and its mix. Under each is a switch for each
    oscillator, for whether it's sent through it, as on the routing screen. Done, or Escape, goes back to the synth.
*/
class EffectsScreen final : public juce::Component
{
public:
    explicit EffectsScreen (SynthProcessor&);

    /** Called when the screen's closed. */
    std::function<void()> onClose;

    /** Brings what's shown up to date with the synth, such as which pitch set the switches set. */
    void refresh();

    /** An effect's dials, in the order shown: the delay's time, its note when synced, its feedback and mix, and the
        reverb's pre-delay, size, decay and mix.
    */
    juce::Slider& getDial (int effect, int index)   { return sections[(size_t) effect].dials[(size_t) index]->knob; }

    /** The switch sending an oscillator through an effect. */
    juce::ToggleButton& getSendSwitch (int effect, int oscillator)   { return sections[(size_t) effect].sends[(size_t) oscillator].sendSwitch; }

    /** The delay's choice of a time in milliseconds, 0, or synced to the tempo, 1. */
    ChoiceButtons& getDelaySyncButtons()   { return delaySync; }

    juce::TextButton doneButton { "Done" };

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Dial
    {
        juce::Slider knob;
        juce::String caption, hint;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Rectangle<int> captionArea, valueArea, hintArea;
    };

    struct Send
    {
        juce::ToggleButton sendSwitch;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        juce::Rectangle<int> labelArea;
    };

    struct Section
    {
        int effect = 0;
        std::vector<Dial*> dials;
        std::array<Send, SynthProcessor::numOscillators> sends;
        juce::Rectangle<int> area, captionArea, sendsCaptionArea, sendsNoteArea;
    };

    Dial& addDial (Section&, const char* parameterId, const juce::String& caption, const juce::String& hint);

    /** The dials shown: the delay's time or note, as it's synced or not. */
    std::vector<Dial*> getShownDials (const Section&) const;
    void showDelaySync();
    void attachSends();

    SynthProcessor& synthProcessor;
    std::deque<Dial> allDials;
    std::array<Section, Patch::numEffects> sections;
    ChoiceButtons delaySync { { "Milliseconds", "Synced" } };
    std::unique_ptr<juce::ParameterAttachment> delaySyncAttachment;
    int attachedPitchSetVersion = -1;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsScreen)
};
