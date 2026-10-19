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
    note by as much as it's set to, added together, then through the note's own high pass and low pass filters.

    Every few milliseconds the voice works out where the LFOs and envelopes have moved the controls, and glides
    there, so changes to the waves, the controls and the modulation never click. Each voice runs the envelopes, and
    the LFOs that run once, for itself, so they start afresh with each note. An envelope moving how loud the note is
    keeps the note sounding until its release is over. Harmonics too high to play without aliasing are faded out.
*/
class HarmonicVoice final : public juce::SynthesiserVoice
{
public:
    explicit HarmonicVoice (const Patch&);

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

    /** How hard the note's key was played, from 0 to 1. */
    float getVelocity() const noexcept   { return noteVelocity; }

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
        std::array<Patch::Filter, modulation::numFilters> filters {};
        float gain = 0.0f;
    };

    Targets getTargets (int startSample) const;
    void renderChunk (juce::AudioBuffer<float>&, int startSample, int numSamples);
    void updateFrequency();

    /** Whether any envelope that moves how loud the note is hasn't finished, so the note must keep sounding. */
    bool isLoudnessEnvelopeRunning() const;
    bool usesLoudnessEnvelope() const;

    const Patch& patch;

    // The amplitudes being played, by oscillator and harmonic, and how far each oscillator is tuned up, all gliding
    // towards the targets.
    Harmonics cosines {}, sines {};
    std::array<double, modulation::numOscillators> tunes {};

    std::array<FilterStage, modulation::numFilters> filters { FilterStage (juce::dsp::StateVariableTPTFilterType::lowpass),
                                                              FilterStage (juce::dsp::StateVariableTPTFilterType::highpass) };
    juce::SmoothedValue<float> gain;

    std::array<EnvelopeRun, modulation::numEnvelopes> envelopeRuns;

    bool startingNote = false;
    bool waitingForEnvelopes = false;   // the key's been let go, but an envelope moving the loudness is still releasing
    juce::int64 samplesSinceNoteOn = 0;
    std::array<double, modulation::numOscillators> phases {};   // how far through its wave's cycle each oscillator is, from 0 to 1
    double frequency = 440.0;
    int noteNumber = 69;
    int pitchWheel = 8192;
    float noteVelocity = 1.0f;
    juce::ADSR ownEnvelope;   // the voice's own, which keeps notes from clicking as they start and stop

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicVoice)
};
