#pragma once

#include <juce_core/juce_core.h>

/** The prompt that teaches an AI, such as Claude in a new Claude Code conversation, everything EZ Synth can do and
    how to write a sound description, so it can make settings that sound like a recorded sample. It's made from the
    synth itself, every parameter with its range, so it's always up to date.
*/
namespace prompt
{
    /** The whole prompt, ready to paste. */
    juce::String make();

    /** The command-line tool that measures samples and plays sound descriptions, as built beside EZ Synth, if it's
        there.
    */
    juce::File getSoundTool();
}
