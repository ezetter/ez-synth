#pragma once

#include "Envelope.h"
#include "FilterStage.h"
#include "Modulation.h"
#include "Tuning.h"

#include <optional>

/** Everything the voices need to play a block: the oscillators, filters and volume as they're set, the LFOs and
    envelopes, and which controls they move. The processor fills it in on the audio thread before each block.
*/
struct Patch
{
    /** The most voices an oscillator's unison can stack. */
    static constexpr int maxUnisonVoices = 8;

    /** How many effects oscillators can be sent through, in a chain, as the filters are, in this order: the delay,
        then the reverb.
    */
    static constexpr int numEffects = 2;
    static constexpr int delayEffect = 0;
    static constexpr int reverbEffect = 1;

    /** The furthest an oscillator's unison can detune its outermost voices, either way, in cents. */
    static constexpr float mostUnisonDetune = 100.0f;

    /** Whether an oscillator slides into each note from the pitch of the note before: never, always, or only when
        played legato, while another key is still held.
    */
    enum class Glide { off, always, legato };

    struct Oscillator
    {
        bool on = false;
        float level = 0.0f;
        int numHarmonics = 1;
        float tune = 0.0f;   // how many semitones away from the note it plays, below it if negative

        /** How far either side its tune slider's zoom reaches, in semitones, which scales how far modulation moves its
            tune: a full range moves it across the part of the scale shown, rather than the whole four octaves.
        */
        float tuneReach = tuning::widestReach;

        /** Whether it slides into each note from the one before, and how long the slide takes. */
        Glide glide = Glide::off;
        float glideSeconds = 0.2f;

        /** Whether it plays white noise rather than its harmonics, at its level. */
        bool noise = false;

        /** Where it's panned, from 0, all the way left, to 1, all the way right. */
        float pan = 0.5f;

        /** How many voices of it play, stacked, how far the outermost are detuned from its pitch, either way, in
            cents, with the others evenly between them, and how widely they're spread either side of its pan, from 0
            to 1, where the lowest is panned all the way left of it and the highest all the way right.
        */
        int unisonVoices = 1;
        float unisonDetune = 20.0f;
        float unisonSpread = 0.5f;

        /** Whether it's sent through each effect, once it's filtered. One sent through none plays straight out. */
        std::array<bool, numEffects> effectSends { true, true };

        /** Whether it's sent through each filter, or skips it, going straight on to the next. */
        std::array<bool, modulation::numFilters> filterSends { true, true, true, true };

        /** How loud it plays panned this far, on the left and on the right: each side at its full level as far as
            the middle, and fading out beyond it, so a sound in the middle plays as it does in mono.
        */
        static float panToLeftGain (float pan)    { return juce::jlimit (0.0f, 1.0f, 2.0f * (1.0f - pan)); }
        static float panToRightGain (float pan)   { return juce::jlimit (0.0f, 1.0f, 2.0f * pan); }

        /** Each harmonic's amplitude, its phase, and the sine and cosine of its phase. */
        std::array<float, waveform::maxHarmonics> amplitudes {}, phases {}, phaseSines {}, phaseCosines {};

        /** Which harmonics are switched off, or on above the number played, and peaks[n], the highest the sum of the
            harmonics played reaches when n are.
        */
        waveform::HarmonicSet switches;
        std::array<float, waveform::maxHarmonics + 1> peaks {};

        /** How far each harmonic, from the 1st up, is tuned from its whole number, in harmonics. */
        std::array<float, waveform::maxHarmonics> tunes {};
    };

    using Filter = FilterStage::Settings;

    struct Lfo
    {
        bool on = false;
        double periodSamples = 48000.0;
        float periodSeconds = 1.0f;

        /** Whether its rate is synced to the host's tempo, so nothing moves it. */
        bool synced = false;

        /** Whether it starts with each note rather than looping all the time, and how many times it plays its wave
            then, or 0 for forever.
        */
        bool fromKey = false;
        int times = 0;

        /** How far through its period a looping LFO is at the start of the block, and how many periods it's been
            through before this one. One that starts with each key starts with each note instead.
        */
        double phase = 0.0;
        std::int64_t cycle = 0;

        waveform::Shape shape {};

        /** Whether it plays a new random wave every cycle, drawn smoothly or in steps, rather than its drawn wave. A
            looping LFO's random wave comes from its seed; one that starts with each key has a new seed with each note.
        */
        bool random = false;
        waveform::Drawing drawing;
        std::uint32_t seed = 0;
    };

    std::array<Oscillator, modulation::numOscillators> oscillators;
    std::array<Filter, modulation::numFilters> filters;
    float volume = 0.75f;
    std::array<Lfo, modulation::numLfos> lfos;
    std::array<EnvelopeSettings, modulation::numEnvelopes> envelopes;

    /** What each control is moved by: an LFO or envelope, its key velocity and the mod wheel, each of which can
        move it.
    */
    modulation::Assignments assignments, velocityAssignments, modWheelAssignments;

    /** Where the mod wheel is, from 0 to 1, as the MIDI played so far has put it. */
    float modWheel = 0.0f;

    /** Set just before a note starts, for its oscillators that glide: the pitch each oscillator of the note played
        before had reached, as a MIDI note number, if a note has been played, and whether another key's still held.
    */
    struct NoteStart
    {
        std::array<std::optional<double>, modulation::numOscillators> glideFrom {};
        bool legato = false;
    };

    NoteStart noteStart;

    /** Where an LFO's rate is set along its dial's travel, from the slowest at 0 to the fastest at 1. */
    float getLfoRatePosition (size_t lfo) const noexcept
    {
        return 1.0f - modulation::periodToPosition (lfos[lfo].periodSeconds);
    }

    /** How long an LFO's period is, in samples, with its rate at a position along its dial's travel. */
    double getLfoPeriodSamplesAt (size_t lfo, float position) const noexcept
    {
        const auto& settings = lfos[lfo];
        const auto moved = juce::jlimit (0.0f, 1.0f, position);
        return settings.periodSamples * (double) (modulation::positionToPeriod (1.0f - moved) / settings.periodSeconds);
    }

    /** How long an LFO's period is now, in samples, for every note: as it's set, or as the mod wheel has moved its
        rate.
    */
    double getLfoPeriodSamples (size_t lfo) const noexcept
    {
        const auto& byModWheel = modWheelAssignments[(size_t) modulation::lfoRateTarget ((int) lfo)];

        if (! byModWheel.isActive() || lfos[lfo].synced)
            return lfos[lfo].periodSamples;

        return getLfoPeriodSamplesAt (lfo, getLfoRatePosition (lfo) + byModWheel.offset (modWheel));
    }

    /** Whether an LFO's rate is moved by something each note has its own of, an LFO, an envelope or velocity, so
        each note runs the LFO at its own rate.
    */
    bool isLfoRateMovedByNote (size_t lfo) const noexcept
    {
        const auto target = (size_t) modulation::lfoRateTarget ((int) lfo);
        return ! lfos[lfo].synced && (assignments[target].isActive() || velocityAssignments[target].isActive());
    }

    /** Whether anything moves a control. */
    bool isMoved (int target) const noexcept
    {
        return assignments[(size_t) target].isActive() || velocityAssignments[(size_t) target].isActive()
            || modWheelAssignments[(size_t) target].isActive();
    }

    /** The volume knob follows a square law, which sounds more even over its turn than a straight line does. */
    static float volumeToGain (float volume)   { return volume * volume; }
};

/** The most pitch sets a sound can have, the default among them. */
inline constexpr int maxPitchSets = 16;

/** A patch for each pitch set, and which plays each key: every setting of the sound can be set differently for a
    range of keys. Each note plays the patch of the pitch set its key is in as it starts, all through.
*/
struct Patches
{
    Patches()
    {
        for (auto& set : setForNote)
            set = 0;
    }

    std::array<Patch, maxPitchSets> sets;
    std::array<std::atomic<int>, 128> setForNote;

    const Patch& forNote (int note) const noexcept
    {
        const auto set = setForNote[(size_t) juce::jlimit (0, 127, note)].load();
        return sets[(size_t) juce::jlimit (0, maxPitchSets - 1, set)];
    }
};
