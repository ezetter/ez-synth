#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <bitset>

/** The on-screen piano, C2 to C7. Each key plays the wave with the key's pitch as its fundamental frequency.
    The computer keys A to ; play the octave from middle C.
*/
class Keyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState&);

    void resized() override;

    /** Marks keys along their foot, in a colour: those the active pitch set plays, or none. */
    void setMarkedKeys (std::bitset<128> keys, juce::Colour);

private:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

    void drawMark (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> key) const;

    std::bitset<128> markedKeys;
    juce::Colour markColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Keyboard)
};
