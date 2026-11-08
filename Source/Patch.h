#pragma once

#include "Envelope.h"
#include "FilterStage.h"
#include "Modulation.h"

/** Everything the voices need to play a block: the oscillators, filters and volume as they're set, the LFOs and
    envelopes, and which controls they move. The processor fills it in on the audio thread before each block.
*/
struct Patch
{
    struct Oscillator
    {
        bool on = false;
        float level = 0.0f;
        int numHarmonics = 1;
        float tune = 0.0f;   // how many semitones away from the note it plays, below it if negative

        /** Each harmonic's amplitude, its phase, and the sine and cosine of its phase. */
        std::array<float, waveform::maxHarmonics> amplitudes {}, phases {}, phaseSines {}, phaseCosines {};

        /** peaks[n] is the highest the sum of the first n harmonics reaches. */
        std::array<float, waveform::maxHarmonics + 1> peaks {};

        /** How far each harmonic, from the 1st up, is tuned from its whole number, in harmonics. */
        std::array<float, waveform::maxHarmonics> tunes {};
    };

    using Filter = FilterStage::Settings;

    struct Lfo
    {
        bool on = false;
        double periodSamples = 48000.0;

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

    /** Whether anything moves a control. */
    bool isMoved (int target) const noexcept
    {
        return assignments[(size_t) target].isActive() || velocityAssignments[(size_t) target].isActive()
            || modWheelAssignments[(size_t) target].isActive();
    }

    /** The volume knob follows a square law, which sounds more even over its turn than a straight line does. */
    static float volumeToGain (float volume)   { return volume * volume; }
};
