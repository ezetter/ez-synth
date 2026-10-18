#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

/** The on-screen piano, C2 to C7. Each key plays the wave with the key's pitch as its fundamental frequency.
    The computer keys A to ; play the octave from middle C.
*/
class Keyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState&);

    void resized() override;

private:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Keyboard)
};
