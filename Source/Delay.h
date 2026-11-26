#pragma once

#include <juce_core/juce_core.h>

#include <vector>

/** A stereo delay for the oscillators sent through it: what goes in comes back after its time, then again and
    again, each repeat the feedback's share of the one before, so at 100% it repeats for ever. What goes round is
    gently saturated, so repeats that build up on one another, at a high feedback, round off rather than growing
    without end.

    Its settings glide to where they're set, so changing them while it repeats doesn't click; a time that glides
    bends the pitch of the repeats a little, as a tape delay's does.
*/
class Delay
{
public:
    struct Settings
    {
        /** How long, in milliseconds, before each repeat. */
        float time = 375.0f;

        /** How much of each repeat comes back again, from 0, only once, to 1, for ever. */
        float feedback = 0.35f;

        /** How much of what comes out is the repeats, from 0 to 1, the rest being what went in. */
        float mix = 0.0f;
    };

    /** The longest it can delay, in milliseconds, such as a whole bar at a slow tempo. A synced time longer than
        this is held to it.
    */
    static constexpr float longestTime = 8000.0f;

    /** The time in milliseconds and the synced lengths it can be set to, of a beat. */
    static constexpr float shortestTime = 1.0f, longestSetTime = 2000.0f;
    static juce::StringArray getDivisionNames();
    static double getDivisionBeats (int division);
    static constexpr int quarterDivision = 8;

    void prepare (double sampleRate);

    /** Lets go of what's repeating, and goes straight to the settings next played, without gliding. */
    void reset();

    /** Plays a stereo pair through it, in place, mixing what went in with the repeats. */
    void process (float* left, float* right, int numSamples, const Settings&);

    /** Whether anything's still repeating, or has been sent in lately. */
    bool isRinging() const noexcept   { return quietSamples < ringingSamples; }

private:
    double sampleRate = 44100.0;
    std::vector<float> leftLine, rightLine;
    int position = 0;

    Settings current;
    bool glide = false, resting = false;
    int quietSamples = 0, ringingSamples = 0;
};
