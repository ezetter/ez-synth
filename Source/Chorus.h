#pragma once

#include <juce_core/juce_core.h>

#include <vector>

/** A stereo chorus for the oscillators sent through it: what goes in comes back a few milliseconds later, the time
    swept up and down by a slow sine LFO, so the copy drifts a little sharp and flat against what went in, as an
    ensemble of players does. The left side's sweep is a quarter of a cycle ahead of the right's, which spreads the
    sound wide.

    The settings are the usual ones: the LFO's rate, its depth, how far the time swings either way, the delay the
    time swings around, the feedback, how much of the copy goes round again, which sharpens it towards a flanger,
    and the mix of the copy with what went in.

    It plays several stereo pairs at once, each with its own delay line, all swept by the one LFO, so every sound
    sent through it, whichever effects it goes on to, moves together. Its settings glide to where they're set, so
    changing them doesn't click.
*/
class Chorus
{
public:
    struct Settings
    {
        /** How fast the time's swept, in hertz. */
        float rate = 0.8f;

        /** How far the time swings either side of the delay, as a share of it, from 0, not at all, to 1, from no
            time to twice the delay.
        */
        float depth = 0.5f;

        /** The time the sweep's centred on, in milliseconds. */
        float delay = 7.0f;

        /** How much of the copy comes back round again, from 0 to most of it. */
        float feedback = 0.0f;

        /** How much of what comes out is the copy, from 0 to 1, the rest being what went in. */
        float mix = 0.0f;
    };

    static constexpr float slowestRate = 0.05f, fastestRate = 10.0f;
    static constexpr float shortestDelay = 1.0f, longestDelay = 40.0f;
    static constexpr float mostFeedback = 0.95f;

    /** One stereo pair it plays, in place. */
    struct Pair
    {
        float* left = nullptr;
        float* right = nullptr;
    };

    void prepare (double sampleRate, int numPairs);

    /** Lets go of what's in its lines, sends the LFO back to the start of its cycle, and goes straight to the
        settings next played, without gliding.
    */
    void reset();

    /** Plays its pairs through it, in place, mixing what went in with the copy. A pair that's silent, with nothing
        left ringing in its line, is skipped.
    */
    void process (const Pair* pairs, int numPairs, int numSamples, const Settings&);

    /** Whether anything's still ringing in a pair's line, or has been sent into it lately. */
    bool isRinging (int pair) const noexcept   { return lanes[(size_t) pair].quietSamples < ringingSamples; }

private:
    struct Lane
    {
        std::vector<float> left, right;
        int quietSamples = 0;
    };

    double sampleRate = 44100.0;
    std::vector<Lane> lanes;
    int position = 0;
    double phase = 0.0;

    Settings current;
    bool glide = false, resting = false;
    int ringingSamples = 0;
};
