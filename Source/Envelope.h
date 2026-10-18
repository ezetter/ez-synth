#pragma once

#include <juce_core/juce_core.h>

/** An envelope's settings: how long its attack, decay and release take, in seconds, and its sustain level, from
    0 to 1.
*/
struct EnvelopeSettings
{
    float attack = 0.01f;
    float decay = 0.3f;
    float sustain = 0.7f;
    float release = 0.5f;
};

/** One note's run through an envelope. When the note starts, the envelope rises from 0 to 1 over its attack, falls
    to its sustain level over its decay, and stays there while the key is held. When the key is let go, it falls from
    wherever it is to 0 over its release. Each stage is a straight line, so it takes just as long as it's set to.
*/
class EnvelopeRun
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    /** Starts the attack, from 0. */
    void start() noexcept;

    /** Starts the release, from wherever the envelope is. */
    void release() noexcept;

    /** Goes straight back to 0, without a release. */
    void stop() noexcept;

    /** Moves the envelope on by some time, with these settings, which can change as it runs. A stage set to take no
        time is passed straight through, even when no time passes.
    */
    void advance (const EnvelopeSettings&, double seconds) noexcept;

    float getLevel() const noexcept   { return level; }
    Stage getStage() const noexcept   { return stage; }
    bool isIdle() const noexcept   { return stage == Stage::idle; }

    /** How far through its current stage the envelope is, from 0 to 1. */
    float getProgress (const EnvelopeSettings&) const noexcept;

private:
    Stage stage = Stage::idle;
    float level = 0.0f;
    float releaseFrom = 0.0f;
};
