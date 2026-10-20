#pragma once

#include "Envelope.h"
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

        /** Each harmonic's amplitude, and the sine and cosine of its phase. */
        std::array<float, waveform::maxHarmonics> amplitudes {}, phaseSines {}, phaseCosines {};

        /** peaks[n] is the highest the sum of the first n harmonics reaches. */
        std::array<float, waveform::maxHarmonics + 1> peaks {};
    };

    struct Filter
    {
        bool on = false;
        float cutoff = 1000.0f;
        float resonance = 0.0f;
    };

    struct Lfo
    {
        bool on = false;
        bool once = false;
        double periodSamples = 48000.0;

        /** How far through its period a looping LFO is at the start of the block. One that runs once starts with
            each note instead.
        */
        double phase = 0.0;

        waveform::Shape shape {};
    };

    std::array<Oscillator, modulation::numOscillators> oscillators;
    std::array<Filter, modulation::numFilters> filters;
    float volume = 0.75f;
    std::array<Lfo, modulation::numLfos> lfos;
    std::array<EnvelopeSettings, modulation::numEnvelopes> envelopes;

    /** What each control is moved by: an LFO or envelope, and its key velocity, each of which can move it. */
    modulation::Assignments assignments, velocityAssignments;

    /** The volume knob follows a square law, which sounds more even over its turn than a straight line does. */
    static float volumeToGain (float volume)   { return volume * volume; }
};
