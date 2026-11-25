#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the window but for the keyboard, setting the effects oscillators can be sent through.
    For now that's the reverb: its pre-delay, the room's size, how long it rings, and how much of it is mixed in,
    with a switch for each oscillator, for whether it's sent through it, as on the routing screen. Done, or Escape,
    goes back to the synth.
*/
class EffectsScreen final : public juce::Component
{
public:
    explicit EffectsScreen (SynthProcessor&);

    /** Called when the screen's closed. */
    std::function<void()> onClose;

    /** Brings which oscillators are sent through the reverb up to date, for the pitch set the controls set. */
    void refresh();

    /** The reverb's dials, its pre-delay, size, decay and mix, and the switch sending an oscillator through it. */
    juce::Slider& getReverbDial (int index)   { return dials[(size_t) index].knob; }
    juce::ToggleButton& getSendSwitch (int oscillator)   { return sends[(size_t) oscillator].sendSwitch; }

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

    void attachSends();

    SynthProcessor& synthProcessor;
    std::array<Dial, 4> dials;
    std::array<Send, SynthProcessor::numOscillators> sends;
    int attachedPitchSetVersion = -1;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, reverbArea, reverbCaptionArea, reverbNoteArea,
                         sendsCaptionArea, sendsNoteArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsScreen)
};
