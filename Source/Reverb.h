#pragma once

#include <array>
#include <vector>

/** A stereo reverb for the oscillators sent through it. The sound waits for the pre-delay, then comes back off the
    walls as a handful of early reflections, then as a dense tail that fades by 60 dB over the decay time, its highs
    fading twice as fast, as they do in a real room. The room's size spaces the reflections and sets the length of
    the echoes the tail is made of, from a small box to a large hall.

    Its settings glide to where they're set, so moving them while it rings doesn't click.
*/
class Reverb
{
public:
    struct Settings
    {
        /** How long, in milliseconds, before the first reflection, from 0 to 200. */
        float delay = 20.0f;

        /** How big the room is, from 1 to 100. */
        float size = 50.0f;

        /** How long, in seconds, the tail takes to fall 60 dB. */
        float decay = 2.0f;

        /** How much of what comes out is the reverb, from 0 to 1, the rest being what went in. */
        float mix = 0.0f;
    };

    static constexpr float longestDelay = 200.0f;
    static constexpr float smallestSize = 1.0f, largestSize = 100.0f;
    static constexpr float shortestDecay = 0.1f, longestDecay = 20.0f;

    void prepare (double sampleRate);

    /** Lets go of what's ringing, and goes straight to the settings next played, without gliding. */
    void reset();

    /** Plays a stereo pair through it, in place, mixing what went in with the reverb. */
    void process (float* left, float* right, int numSamples, const Settings&);

    /** Whether anything's still ringing, or has been sent in lately. */
    bool isRinging() const noexcept   { return quietSamples < ringingSamples; }

private:
    static constexpr int numLines = 8;

    struct Line
    {
        std::vector<float> buffer;
        int position = 0;
        float filtered = 0.0f;

        void write (float value) noexcept;
        float read (float delaySamples) const noexcept;
    };

    struct Diffuser
    {
        Line line;
        float baseMilliseconds = 0.0f;
        float process (float input, float delaySamples) noexcept;
    };

    void updateTail();

    double sampleRate = 44100.0;
    Line leftIn, rightIn;
    std::array<Line, numLines> lines;
    std::array<Diffuser, 4> diffusers;

    // Where the settings are, gliding to where they're set, and the tail's gains for them.
    Settings current;
    bool glide = false;
    float scale = 1.0f;
    std::array<float, numLines> lengths {}, gains {}, poles {};
    float inputGain = 0.0f;
    int untilUpdate = 0;

    int quietSamples = 0, ringingSamples = 0;
    bool resting = false;
};
