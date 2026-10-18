#pragma once

#include "Waveform.h"

#include <juce_audio_basics/juce_audio_basics.h>

/** The sine waves every voice plays: those of all the oscillators that are on, each oscillator's scaled so that
    its sum peaks at its level, added together harmonic by harmonic.
    value (t) = sum over k of cosines[k] * cos (2π k t) + sines[k] * sin (2π k t).
    The processor updates them on the audio thread, between blocks.
*/
struct HarmonicTargets
{
    std::array<float, waveform::maxHarmonic + 1> cosines {}, sines {};

    /** Goes up whenever the amplitudes change. */
    int version = 0;
};

struct HarmonicSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override      { return true; }
    bool appliesToChannel (int) override   { return true; }
};

/** Plays a note as a sum of sine waves at the note's harmonics.

    When the wave changes during a note, the voice glides to the new one over a few milliseconds rather than
    clicking. Harmonics that would be too high to play without aliasing are faded out.
*/
class HarmonicVoice final : public juce::SynthesiserVoice
{
public:
    explicit HarmonicVoice (const HarmonicTargets&);

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

private:
    void glideToTargets();
    void updateFrequency();

    const HarmonicTargets& targets;
    int targetsVersion = -1;

    // The amplitudes being played, by harmonic, and how much each moves per sample while gliding to the targets.
    std::array<float, waveform::maxHarmonic + 1> cosines {}, sines {}, cosineSteps {}, sineSteps {};
    int glideSamplesLeft = 0;

    double phase = 0.0;   // how far through the wave's cycle the voice is, from 0 to 1
    double frequency = 440.0;
    int noteNumber = 69;
    int pitchWheel = 8192;
    float level = 0.0f;
    juce::ADSR envelope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicVoice)
};
