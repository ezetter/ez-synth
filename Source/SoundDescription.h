#pragma once

#include "SynthProcessor.h"

/** A whole sound written out as JSON, in terms a person or an AI can read and write: each control by its parameter
    ID in its own units, each oscillator's harmonics, each LFO's wave as a preset or as points, and what moves what.
    It's how Claude hands EZ Synth a sound it's made to match a sample, and how EZ Synth shows Claude a sound.

    A description only needs what differs from Init: everything it leaves out is as Init has it. Its sound is the
    default pitch set's, and it can list other pitch sets, each with its range of keys and a sound of its own, in
    the same terms, also starting from Init.
*/
namespace sound
{
    /** The value of the description's "format" field. */
    inline constexpr auto formatName = "EZ Synth sound";
    /** Version 2 since oscillators can be tuned two octaves away, which changed what a tune's zoom means, and 3
        since a sound can have pitch sets, each a sound of its own for a range of keys.
    */
    inline constexpr int formatVersion = 3;

    /** What loading a description did: whether it loaded at all, and anything in it that was left out because it
        couldn't be understood, such as a parameter ID that doesn't exist, each said in a line.
    */
    struct LoadResult
    {
        bool loaded = false;
        juce::StringArray problems;
    };

    /** Loads a sound from its description, starting from Init, and names it after the description's name. A
        description that isn't JSON, or isn't an object, leaves the sound alone.
    */
    LoadResult load (SynthProcessor&, const juce::String& json);

    /** Loads a sound from a file holding its description. */
    LoadResult loadFile (SynthProcessor&, const juce::File&);

    /** Describes the sound as it is, with every pitch set. With onlyChanged, the parameters as Init has them are left
        out, and so are the LFOs that are off and move nothing, so the description is short. Each pitch set is made
        active in turn to describe it, then the one that was is again.
    */
    juce::var describe (SynthProcessor&, bool onlyChanged);
    juce::String toJson (SynthProcessor&, bool onlyChanged);

    /** A key, as a MIDI note number or a note name with middle C as C4, such as "C2", "F#3" or "Bb4", or nothing if
        it's neither.
    */
    std::optional<int> findNote (const juce::var&);

    /** The folder EZ Synth looks in for sounds from Claude: ~/Documents/EZ Synth/From Claude. */
    juce::File getClaudeFolder();

    /** The sound descriptions in that folder, newest first. */
    juce::Array<juce::File> findClaudeSounds (const juce::File& folder = getClaudeFolder());
}
