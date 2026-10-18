#pragma once

#include "Waveform.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <optional>

/** The controls the LFOs can move, and how each moves one. */
namespace modulation
{
    inline constexpr int numLfos = 3;
    inline constexpr int numOscillators = 3;

    /** The filters, in the order they're numbered: the low pass and the high pass. */
    inline constexpr int numFilters = 2;
    inline constexpr int lowPass = 0;
    inline constexpr int highPass = 1;

    // Every control an LFO can move is a target, with a number.
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

    /** How an LFO moves a control. */
    struct Assignment
    {
        /** The LFO that moves it, or -1 if none does. */
        int lfo = -1;

        /** How far the control swings, from one end to the other, as a proportion of its travel. It swings
            evenly either side of where it's set.
        */
        float range = 0.5f;

        /** Whether the control falls as the LFO rises, rather than rising with it. */
        bool inverted = false;

        bool isActive() const noexcept   { return lfo >= 0; }

        /** Where an LFO at a value from -1 to 1 moves a control set at a position from 0 to 1. */
        float apply (float position, float lfoValue) const noexcept
        {
            return juce::jlimit (0.0f, 1.0f, position + (inverted ? -0.5f : 0.5f) * range * lfoValue);
        }

        bool operator== (const Assignment& other) const noexcept
        {
            return lfo == other.lfo && juce::exactlyEqual (range, other.range) && inverted == other.inverted;
        }
    };

    using Assignments = std::array<Assignment, numTargets>;

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

    //==============================================================================
    /** Where an LFO is, given how far through its periods it's gone. A looping LFO repeats; one that runs once
        holds its wave's last value after its period is over.
    */
    float lfoValue (const waveform::Shape&, double periodsGone, bool once);
}
