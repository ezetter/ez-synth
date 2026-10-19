#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MidiInputs.h"

/** In the standalone app, the MIDI devices it's playing from, with a light that flashes as MIDI arrives. Clicking it
    lists the devices plugged in, to switch each one on or off, and opens the audio and MIDI settings.
*/
class MidiPanel final : public juce::Component
{
public:
    MidiPanel();

    /** Brings the devices shown up to date, and lights up if MIDI has arrived since last time: the processor's count
        of blocks that brought MIDI.
    */
    void refresh (int midiReceived);

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** What it says about the devices: their names, or how many there are, and whether any is switched on. */
    static juce::String describe (const std::vector<MidiInputs::Input>&);

private:
    std::vector<MidiInputs::Input> inputs;
    int lastReceived = 0;
    juce::uint32 litUntil = 0;
    bool lit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPanel)
};
