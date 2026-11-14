#pragma once

#include "SynthProcessor.h"

/** A whole sound written out as JSON, in terms a person or an AI can read and write: each control by its parameter
    ID in its own units, each oscillator's harmonics, each LFO's wave as a preset or as points, and what moves what.
    It's how Claude hands EZ Synth a sound it's made to match a sample, and how EZ Synth shows Claude a sound.

    A description only needs what differs from Init: everything it leaves out is as Init has it.
*/
namespace sound
{
    /** The value of the description's "format" field. */
    inline constexpr auto formatName = "EZ Synth sound";
    inline constexpr int formatVersion = 1;

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

    /** Describes the sound as it is. With onlyChanged, the parameters as Init has them are left out, and so are the
        LFOs that are off and move nothing, so the description is short.
    */
    juce::var describe (const SynthProcessor&, bool onlyChanged);
    juce::String toJson (const SynthProcessor&, bool onlyChanged);

    /** The folder EZ Synth looks in for sounds from Claude: ~/Documents/EZ Synth/From Claude. */
    juce::File getClaudeFolder();

    /** The sound descriptions in that folder, newest first. */
    juce::Array<juce::File> findClaudeSounds (const juce::File& folder = getClaudeFolder());
}
