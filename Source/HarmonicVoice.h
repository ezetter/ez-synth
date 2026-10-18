#pragma once

#include "FilterStage.h"
#include "Patch.h"

#include <juce_audio_basics/juce_audio_basics.h>

struct HarmonicSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override      { return true; }
    bool appliesToChannel (int) override   { return true; }
};

/** Plays a note: the sine waves at the harmonics of every oscillator that's on, added together, then through the
    note's own high pass and low pass filters.

    Every few milliseconds the voice works out where the LFOs have moved the controls, and glides there, so
    changes to the waves, the controls and the LFOs never click. Each voice runs the LFOs for itself, so one that
    runs once starts afresh with each note, like an envelope. Harmonics too high to play without aliasing are faded
    out.
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

    /** How many samples the voice plays before working out the LFOs again. */
    static constexpr int controlInterval = 64;

private:
    /** Where the voice is heading: its harmonics, its filters and its loudness, with the LFOs applied. */
    struct Targets
    {
        std::array<float, waveform::maxHarmonics + 1> cosines {}, sines {};
        std::array<Patch::Filter, modulation::numFilters> filters {};
        float gain = 0.0f;
    };

    Targets getTargets (int startSample) const;
    void renderChunk (juce::AudioBuffer<float>&, int startSample, int numSamples);
    void updateFrequency();

    const Patch& patch;

    // The amplitudes being played, by harmonic, gliding towards the targets.
    std::array<float, waveform::maxHarmonics + 1> cosines {}, sines {};

    std::array<FilterStage, modulation::numFilters> filters { FilterStage (juce::dsp::StateVariableTPTFilterType::lowpass),
                                                              FilterStage (juce::dsp::StateVariableTPTFilterType::highpass) };
    juce::SmoothedValue<float> gain;

    bool startingNote = false;
    juce::int64 samplesSinceNoteOn = 0;
    double phase = 0.0;   // how far through the wave's cycle the voice is, from 0 to 1
    double frequency = 440.0;
    int noteNumber = 69;
    int pitchWheel = 8192;
    float level = 0.0f;
    juce::ADSR envelope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicVoice)
};
