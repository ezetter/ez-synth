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

    /** The two filters, numbered from 0, each of which can be a low pass, a mid pass or a high pass. */
    inline constexpr int numFilters = 2;

    // Every control an LFO or envelope can move is a target, with a number.
    inline constexpr int volumeTarget = 0;
    constexpr int oscillatorOnTarget (int oscillator)               { return 1 + oscillator; }
    constexpr int oscillatorLevelTarget (int oscillator)            { return 4 + oscillator; }
    constexpr int oscillatorHarmonicsTarget (int oscillator)        { return 7 + oscillator; }
    constexpr int harmonicTarget (int oscillator, int harmonic)     { return 10 + oscillator * waveform::maxHarmonics + harmonic - 1; }
    constexpr int filterOnTarget (int filter)                       { return 37 + filter; }
    constexpr int filterCutoffTarget (int filter)                   { return 39 + filter; }
    constexpr int filterResonanceTarget (int filter)                { return 41 + filter; }
    constexpr int oscillatorTuneTarget (int oscillator)             { return 43 + oscillator; }
    constexpr int filterWidthTarget (int filter)                    { return 46 + filter; }
    constexpr int filterQTarget (int filter)                        { return 48 + filter; }
    constexpr int filterDepthTarget (int filter)                    { return 50 + filter; }

    /** The tune of a harmonic above the fundamental, from the 2nd up. */
    constexpr int harmonicTuneTarget (int oscillator, int harmonic) { return 52 + oscillator * (waveform::maxHarmonics - 1) + harmonic - 2; }

    /** The phase of a harmonic, from the 1st up, from 0 at the bottom of its travel round to a whole turn at the top. */
    constexpr int harmonicPhaseTarget (int oscillator, int harmonic) { return 76 + oscillator * waveform::maxHarmonics + harmonic - 1; }
    inline constexpr int numTargets = 103;

    enum class Kind { volume, oscillatorOn, oscillatorLevel, oscillatorHarmonics, harmonic, filterOn, filterCutoff, filterResonance,
                      oscillatorTune, filterWidth, filterQ, filterDepth, harmonicTune, harmonicPhase };

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

    /** The target with a key, including the keys saved before the filters could each be any type, when the first
        was always a low pass and the second a high pass, such as "lowPassCutoff".
    */
    std::optional<int> findTarget (const juce::String& key);

    /** A key, or a parameter ID, as it's been since the filters could each be any type: "lowPassCutoff" becomes
        "filter1Cutoff", and "highPassOn" becomes "filter2On". Any other key is left as it is.
    */
    juce::String updateKey (const juce::String& key);

    /** Whether a target is a plugin parameter, rather than one of a wave's harmonics or their tunes or phases. */
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

        /** For an LFO, how far its wave is shifted to the right for this control, as a proportion of its period, from
            0, in step with the LFO, up to just short of a whole period. The control follows where the LFO was that
            long ago, so controls on the same LFO can move out of step with each other.
        */
        float phase = 0.0f;

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
            return source == other.source && juce::exactlyEqual (range, other.range) && inverted == other.inverted
                && juce::exactlyEqual (phase, other.phase);
        }
    };

    /** Where an envelope is in a note: its level, and once the note's key has been let go, the level it was at then
        and how much of its release is still to come, from 1 to 0, which is 0 once the release is over.
    */
    struct EnvelopeState
    {
        float level = 0.0f;
        bool releasing = false;
        float releaseFrom = 0.0f;
        float releaseLeft = 1.0f;
    };

    /** Where an envelope's release takes a control: the bottom of its travel, or for an oscillator's or a harmonic's
        tune, in tune.
    */
    float getRestPosition (int target);

    /** Where an envelope moves a control to, from a position along its travel. While the key is held, the envelope
        moves it by its range times its level. Once the key is let go, the control falls steadily from where it was
        then to its rest, which it reaches as the release ends, so an envelope on the volume fades the note out.
    */
    float applyEnvelope (int target, const Assignment&, float position, const EnvelopeState&);

    /** The furthest an LFO's wave can be shifted for a control: a degree short of a whole period. */
    inline constexpr float latestPhase = 359.0f / 360.0f;

    /** A phase, from 0 to 1, in whole degrees, such as "90°". */
    juce::String phaseToText (float phase);

    using Assignments = std::array<Assignment, numTargets>;

    /** Velocity starts out moving the volume, so softer notes are quieter, as they are on most instruments. */
    inline constexpr Assignment startingVelocityOnVolume { velocitySource, 0.35f, false };

    //==============================================================================
    // Where controls sit along their travel, from 0 to 1, and back. These are safe on the audio thread.

    /** The travel of a harmonic's slider, which gives weak harmonics more of its length. */
    const juce::NormalisableRange<float>& amplitudeRange();

    float harmonicsToPosition (int numHarmonics);
    int positionToHarmonics (float position);

    /** An LFO's period, from 0.02 seconds, 50 Hz, to 10 seconds, spread evenly in proportion, as tempo is felt. */
    float periodToPosition (float seconds);
    float positionToPeriod (float position);
    inline constexpr float shortestPeriod = 0.02f;
    inline constexpr float longestPeriod = 10.0f;

    /** The longest an envelope's attack, decay or release can take, in seconds. */
    inline constexpr float longestStage = 20.0f;

    //==============================================================================
    /** Where an LFO is, given how far through its periods it's gone. A looping LFO repeats; one that runs once
        holds its wave's last value after its period is over.
    */
    float lfoValue (const waveform::Shape&, double periodsGone, bool once);
}
