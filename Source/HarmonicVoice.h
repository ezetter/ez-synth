#pragma once

#include "FilterStage.h"
#include "Patch.h"

#include <juce_audio_basics/juce_audio_basics.h>

struct HarmonicSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override      { return true; }
    bool appliesToChannel (int) override   { return true; }
};

/** Plays a note: the sine waves at the harmonics of every oscillator that's on, each oscillator tuned up from the
    note by as much as it's set to, added together, then through the note's own two filters, in turn.

    Every few milliseconds the voice works out where the LFOs and envelopes have moved the controls, and glides
    there, so changes to the waves, the controls and the modulation never click. Each voice runs the envelopes, and
    the LFOs that run once, for itself, so they start afresh with each note. An envelope moving how loud the note is
    keeps the note sounding until its release is over. Harmonics too high to play without aliasing are faded out.
*/
class HarmonicVoice final : public juce::SynthesiserVoice
{
public:
    /** Plays each note with the patch of the pitch set its key is in. */
    explicit HarmonicVoice (const Patches&);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void setCurrentPlaybackSampleRate (double newRate) override;

    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using SynthesiserVoice::renderNextBlock;

    /** How long a note takes to fade away after its key is let go. */
    static constexpr float releaseSeconds = 0.25f;

    /** Harmonics fade out above this frequency, which keeps them clear of the Nyquist frequency, where they'd alias. */
    static constexpr double fadeStartFrequency = 16000.0;

    /** Harmonics this high or higher aren't played. */
    static constexpr double highestFrequency = 19000.0;

    /** How many samples the voice plays before working out the LFOs and envelopes again. */
    static constexpr int controlInterval = 64;

    /** This note's run through an envelope, for the window to show. */
    const EnvelopeRun& getEnvelopeRun (int index) const noexcept   { return envelopeRuns[(size_t) index]; }

    /** How long ago the note started, so the window can show the newest note's envelopes. */
    juce::int64 getSamplesSinceNoteOn() const noexcept   { return samplesSinceNoteOn; }

    /** The patch the note is playing, its pitch set's. */
    const Patch& getPatch() const noexcept   { return *patch; }

    /** How hard the note's key was played, from 0 to 1. */
    float getVelocity() const noexcept   { return noteVelocity; }

    /** How many periods an LFO has been through, as this note plays it. */
    double getLfoPeriods (int lfo) const noexcept   { return lfoPeriods[(size_t) lfo]; }

    /** The pitch an oscillator is playing, as a MIDI note number, as it glides into the note, or the note's own once
        it's got there or if it doesn't glide. It leaves out the oscillator's tune and the pitch wheel.
    */
    double getGlidingPitch (int oscillator) const noexcept;

    /** The seed of the random wave an LFO that starts with each key plays for this note, new with each note. */
    std::uint32_t getRandomSeed (int lfo) const noexcept   { return randomSeeds[(size_t) lfo]; }

    /** How many of the samples of white noise an oscillator played last it keeps, for the window to show. */
    static constexpr int recentNoiseLength = 512;

    /** How many channels an output needs for the voices to play each effect bus on its own pair, after the pair
        played straight out. With fewer, every oscillator plays straight out.
    */
    static constexpr int routedChannels = 2 * (1 + Patch::numEffectBuses);

    /** Copies the last samples of white noise an oscillator played, from -1 to 1 before its level, oldest first. */
    void copyRecentNoise (int oscillator, float* destination, int count) const noexcept;

private:
    /** Each oscillator's harmonics, from the 1st up, as the amplitudes of the cosine and sine waves that make them. */
    using Harmonics = std::array<std::array<float, waveform::maxHarmonics + 1>, modulation::numOscillators>;

    /** Where the voice is heading: its oscillators' harmonics and tunings, its filters and its loudness, with the
        LFOs applied.
    */
    struct Targets
    {
        Harmonics cosines {}, sines {};
        std::array<float, modulation::numOscillators> tunes {};

        /** How far each harmonic is tuned from its whole number, in harmonics. */
        Harmonics harmonicTunes {};

        /** How loud each oscillator playing white noise is, or 0 for one playing harmonics. */
        std::array<float, modulation::numOscillators> noiseLevels {};

        /** Where each oscillator is panned, from 0, left, to 1, right, as it's moved, and whether anything moves it. */
        std::array<float, modulation::numOscillators> pans {};
        std::array<bool, modulation::numOscillators> pansMoved {};

        /** How many voices of each oscillator's unison play, how far the outermost are detuned, in semitones, and how
            widely they're spread, from 0 to 1, and whether anything moves the number or the spread.
        */
        std::array<int, modulation::numOscillators> unisonVoices {};
        std::array<float, modulation::numOscillators> unisonDetunes {}, unisonSpreads {};
        std::array<bool, modulation::numOscillators> unisonMoved {};

        /** How much of each oscillator is sent through each filter: 1 if it is, 0 if it skips it. */
        std::array<std::array<float, modulation::numFilters>, modulation::numOscillators> sends {};

        /** Whether each oscillator's sent through each effect bus. */
        std::array<std::array<bool, (size_t) Patch::numEffectBuses>, modulation::numOscillators> busSends {};
        std::array<Patch::Filter, modulation::numFilters> filters {};
        float gain = 0.0f;

        /** How many periods each LFO has been through, and how long its period is now, in samples, for the note to
            move each on by.
        */
        std::array<double, modulation::numLfos> lfoPeriods {}, lfoPeriodSamples {};
    };

    Targets getTargets (int startSample) const;
    void renderChunk (juce::AudioBuffer<float>&, int startSample, int numSamples);
    void updateFrequency();

    /** Whether any envelope that moves how loud the note is hasn't finished, so the note must keep sounding. */
    bool isLoudnessEnvelopeRunning() const;
    bool usesLoudnessEnvelope() const;

    /** Whether an envelope moves a control. */
    bool hasEnvelope (int target) const;

    const Patches& patches;
    const Patch* patch = &patches.sets[0];   // the note's pitch set's, chosen as it starts

    // The amplitudes being played, by oscillator and harmonic, and how far each oscillator is tuned up, all gliding
    // towards the targets.
    Harmonics cosines {}, sines {};
    std::array<double, modulation::numOscillators> tunes {};

    // How far each harmonic is tuned from its whole number, gliding towards the targets, and how far its phase has
    // drifted from its whole number's because of it, from 0 to 1. A harmonic in tune hasn't drifted, so it keeps
    // its place in the wave.
    Harmonics harmonicTunes {};
    std::array<std::array<double, waveform::maxHarmonics + 1>, modulation::numOscillators> harmonicDrifts {};

    std::array<FilterStage, modulation::numFilters> filters;

    // How much of each oscillator is sent through each filter, gliding over 20 ms when a send's switched, so it
    // doesn't click. Oscillators sent alike are filtered together, in a lane led by the first of them, with the
    // lead's memories of the sound, left and right, in each filter. Each oscillator's lead last chunk, or -1 if it
    // wasn't sounding, lets a lane that's led by another oscillator now carry on from the memories it had.
    std::array<std::array<float, modulation::numFilters>, modulation::numOscillators> sendLevels {};

    // Where each oscillator's sound goes once it's filtered, gliding over 20 ms when it's switched: straight out, the
    // first, or through each effect bus. Each goes to its own pair of the output's channels, when the output has
    // enough for every bus; otherwise everything goes straight out.
    static constexpr size_t numRoutes = 1 + (size_t) Patch::numEffectBuses;
    std::array<std::array<float, numRoutes>, modulation::numOscillators> routeLevels {};

    struct LaneMemory
    {
        FilterStage::Memory left, right;
    };

    std::array<std::array<LaneMemory, modulation::numFilters>, modulation::numOscillators> laneMemories {};
    std::array<int, modulation::numOscillators> previousLeads {};
    juce::SmoothedValue<float> gain;

    // How loud each oscillator's white noise is, gliding towards its target, the noise's generator, and the last
    // samples of it each oscillator played, in a ring that's written at the position given.
    std::array<float, modulation::numOscillators> noiseLevels {};

    // Each oscillator plays as many voices as its unison has, each a copy of its harmonics, detuned and panned
    // apart from the others. Each copy is a unit, numbered from the oscillator's first. A copy fades in or out as
    // the number of voices changes, and glides to its detune.
    static constexpr size_t maxCopies = (size_t) Patch::maxUnisonVoices;
    static constexpr size_t numUnits = (size_t) modulation::numOscillators * maxCopies;
    std::array<std::array<double, maxCopies>, modulation::numOscillators> copyDetunes {};
    std::array<std::array<float, maxCopies>, modulation::numOscillators> copyGains {};

    // Where each copy of each oscillator is panned, gliding quickly to where it's set, so moving it doesn't click.
    std::array<float, numUnits> pans {};
    std::uint32_t noiseState = 0x9e3779b9u;
    std::array<std::array<float, (size_t) recentNoiseLength>, modulation::numOscillators> recentNoise {};
    size_t recentNoisePosition = 0;

    std::array<EnvelopeRun, modulation::numEnvelopes> envelopeRuns;

    bool startingNote = false;
    bool waitingForEnvelopes = false;   // the key's been let go, but an envelope moving the loudness is still releasing
    juce::int64 samplesSinceKeyUp = 0;   // while waiting for the envelopes
    juce::int64 samplesSinceNoteOn = 0;

    /** How many periods each LFO has been through as this note plays it. One that starts with each key counts from
        the note's start. A looping one is where every note has it, unless its rate is moved by something the note
        has its own of, when it carries on from where it was as the note started, at the note's own rate.
    */
    std::array<double, modulation::numLfos> lfoPeriods {};
    // How far through its wave's cycle each copy of each oscillator is, from 0 to 1.
    std::array<std::array<double, maxCopies>, modulation::numOscillators> phases {};

    /** A harmonic of a copy of an oscillator that's playing, or gliding in or out, as a chunk plays it. */
    struct Playing
    {
        float cosine = 0.0f, sine = 0.0f, cosineStep = 0.0f, sineStep = 0.0f, bandLimit = 0.0f;
        double x = 0.0, y = 0.0, turnX = 0.0, turnY = 0.0;
        size_t unit = 0;
    };

    std::array<Playing, numUnits * (size_t) waveform::maxHarmonics> playing;
    double frequency = 440.0;

    /** How far below or above the note, in semitones, each oscillator started as it glides into it, and how long
        its glide takes, in samples. One that doesn't glide starts at the note.
    */
    std::array<double, modulation::numOscillators> glideStarts {}, glideLengths {};
    int noteNumber = 69;
    int pitchWheel = 8192;
    float noteVelocity = 1.0f;
    juce::ADSR ownEnvelope;   // the voice's own, which keeps notes from clicking as they start and stop
    std::array<std::uint32_t, modulation::numLfos> randomSeeds {};
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicVoice)
};
