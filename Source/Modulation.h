#pragma once

#include "Waveform.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <optional>

/** The controls the LFOs, envelopes and key velocity can move, and how each moves one. */
namespace modulation
{
    inline constexpr int numLfos = 3;
    inline constexpr int numEnvelopes = 3;
    inline constexpr int numOscillators = 3;

    /** What moves controls: the LFOs, then the envelopes, numbered in that order. */
    inline constexpr int numSources = numLfos + numEnvelopes;
    constexpr bool isEnvelope (int source)          { return source >= numLfos && source < numSources; }
    constexpr int envelopeSource (int envelope)     { return numLfos + envelope; }

    /** How hard each key is played can move a control too, as well as an LFO or envelope moving it. */
    inline constexpr int velocitySource = numSources;

    /** What a source is called, such as "LFO 2", "Env 1" or "Velocity". */
    juce::String getSourceName (int source);

    /** The filters, in the order they're numbered: the low pass and the high pass. */
    inline constexpr int numFilters = 2;
    inline constexpr int lowPass = 0;
    inline constexpr int highPass = 1;

    // Every control an LFO or envelope can move is a target, with a number.
    inline constexpr int volumeTarget = 0;
    constexpr int oscillatorOnTarget (int oscillator)               { return 1 + oscillator; }
    constexpr int oscillatorLevelTarget (int oscillator)            { return 4 + oscillator; }
    constexpr int oscillatorHarmonicsTarget (int oscillator)        { return 7 + oscillator; }
    constexpr int harmonicTarget (int oscillator, int harmonic)     { return 10 + oscillator * waveform::maxHarmonics + harmonic - 1; }
    constexpr int filterOnTarget (int filter)                       { return 37 + filter; }
    constexpr int filterCutoffTarget (int filter)                   { return 39 + filter; }
    constexpr int filterResonanceTarget (int filter)                { return 41 + filter; }
    inline constexpr int numTargets = 43;

    enum class Kind { volume, oscillatorOn, oscillatorLevel, oscillatorHarmonics, harmonic, filterOn, filterCutoff, filterResonance };

    struct TargetInfo
    {
        Kind kind = Kind::volume;
        int oscillator = 0;
        int harmonic = 1;
        int filter = 0;
    };

    TargetInfo describe (int target);

    /** What the control is called, such as "Osc 2 harmonic 3". */
    juce::String getName (int target);

    /** A name for the target that stays the same from version to version, for saving. Targets that are plugin
        parameters use the parameter's ID.
    */
    juce::String getKey (int target);
    std::optional<int> findTarget (const juce::String& key);

    /** Whether a target is a plugin parameter, rather than one of a wave's harmonics. */
    bool isParameter (int target);

    /** Whether a target sets how loud the sound is: the volume, or an oscillator's switch or level. A note goes on
        sounding until an envelope moving one of these has finished.
    */
    bool isLoudness (int target);

    /** How an LFO, envelope or key velocity moves a control. */
    struct Assignment
    {
        /** The LFO, envelope or velocity that moves it, or -1 if nothing does. */
        int source = -1;

        /** How far the control moves, from one end of the source's values to the other, as a proportion of its
            travel. An LFO, from -1 to 1, swings the control evenly either side of where it's set. An envelope, from
            0 to 1, moves it one way from where it's set and back. Velocity, from 0 to 1, leaves the control where
            it's set for the hardest notes, and moves it down for softer ones.
        */
        float range = 0.5f;

        /** Whether the control falls as the source rises, rather than rising with it. */
        bool inverted = false;

        bool isActive() const noexcept   { return source >= 0; }

        /** How far the source at a value moves a control along its travel, from where it's set. */
        float offset (float sourceValue) const noexcept
        {
            const auto amount = source == velocitySource ? sourceValue - 1.0f
                                                         : (isEnvelope (source) ? sourceValue : 0.5f * sourceValue);
            return (inverted ? -1.0f : 1.0f) * range * amount;
        }

        /** Where the source at a value moves a control set at a position from 0 to 1. */
        float apply (float position, float sourceValue) const noexcept
        {
            return juce::jlimit (0.0f, 1.0f, position + offset (sourceValue));
        }

        bool operator== (const Assignment& other) const noexcept
        {
            return source == other.source && juce::exactlyEqual (range, other.range) && inverted == other.inverted;
        }
    };

    using Assignments = std::array<Assignment, numTargets>;

    /** Velocity starts out moving the volume, so softer notes are quieter, as they are on most instruments. */
    inline constexpr Assignment startingVelocityOnVolume { velocitySource, 0.35f, false };

    //==============================================================================
    // Where controls sit along their travel, from 0 to 1, and back. These are safe on the audio thread.

    /** The travel of a harmonic's slider, which gives weak harmonics more of its length. */
    const juce::NormalisableRange<float>& amplitudeRange();

    float harmonicsToPosition (int numHarmonics);
    int positionToHarmonics (float position);

    /** An LFO's period, from 0.1 to 10 seconds, spread evenly in proportion, as tempo is felt. */
    float periodToPosition (float seconds);
    float positionToPeriod (float position);
    inline constexpr float shortestPeriod = 0.1f;
    inline constexpr float longestPeriod = 10.0f;

    /** The longest an envelope's attack, decay or release can take, in seconds. */
    inline constexpr float longestStage = 20.0f;

    //==============================================================================
    /** Where an LFO is, given how far through its periods it's gone. A looping LFO repeats; one that runs once
        holds its wave's last value after its period is over.
    */
    float lfoValue (const waveform::Shape&, double periodsGone, bool once);
}
