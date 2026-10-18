#include "HarmonicVoice.h"

namespace
{
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;

    /** How loud a note is at full velocity, leaving headroom for chords. */
    constexpr float fullLevel = 0.3f;

    /** How long a playing note takes to glide to a changed wave. */
    constexpr double glideSeconds = 0.01;

    /** How much of a harmonic at this frequency to play. At low sample rates the fade starts lower, to stay clear
        of the Nyquist frequency.
    */
    float bandLimitGain (double frequency, double sampleRate)
    {
        const auto fadeStart = std::min (HarmonicVoice::fadeStartFrequency, 0.40 * sampleRate);
        const auto fadeEnd = std::min (HarmonicVoice::highestFrequency, 0.47 * sampleRate);

        if (frequency <= fadeStart)
            return 1.0f;

        if (frequency >= fadeEnd)
            return 0.0f;

        return (float) ((fadeEnd - frequency) / (fadeEnd - fadeStart));
    }
}

HarmonicVoice::HarmonicVoice (const HarmonicTargets& targetsToPlay)
    : targets (targetsToPlay)
{
    envelope.setParameters ({ 0.008f, 0.05f, 1.0f, releaseSeconds });
}

bool HarmonicVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<HarmonicSound*> (sound) != nullptr;
}

void HarmonicVoice::setCurrentPlaybackSampleRate (double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate > 0.0)
        envelope.setSampleRate (newRate);
}

void HarmonicVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    noteNumber = midiNoteNumber;
    pitchWheel = currentPitchWheelPosition;
    updateFrequency();

    phase = 0.0;
    level = fullLevel * (0.3f + 0.7f * velocity);

    // A new note starts on the current wave, with nothing to glide from.
    cosines = targets.cosines;
    sines = targets.sines;
    glideSamplesLeft = 0;
    targetsVersion = targets.version;

    envelope.noteOn();
}

void HarmonicVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        envelope.noteOff();
    }
    else
    {
        envelope.reset();
        clearCurrentNote();
    }
}

void HarmonicVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchWheel = newPitchWheelValue;
    updateFrequency();
}

void HarmonicVoice::updateFrequency()
{
    // The pitch wheel bends up to two semitones either way.
    const auto bend = 2.0 * (pitchWheel - 8192) / 8192.0;
    frequency = 440.0 * std::pow (2.0, (noteNumber - 69 + bend) / 12.0);
}

void HarmonicVoice::glideToTargets()
{
    const auto glideSamples = juce::jmax (1, juce::roundToInt (glideSeconds * getSampleRate()));

    for (size_t k = 1; k < cosines.size(); ++k)
    {
        cosineSteps[k] = (targets.cosines[k] - cosines[k]) / (float) glideSamples;
        sineSteps[k] = (targets.sines[k] - sines[k]) / (float) glideSamples;
    }

    glideSamplesLeft = glideSamples;
    targetsVersion = targets.version;
}

void HarmonicVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    if (targets.version != targetsVersion)
        glideToTargets();

    const auto sampleRate = getSampleRate();
    const auto cyclesPerSample = frequency / sampleRate;

    // Each harmonic is a point going round a circle, turned by a fixed angle every sample, which is much cheaper
    // than calling sin() every sample. The points are placed afresh from the phase each block, so rounding errors
    // can't build up. Only the harmonics that are playing, or gliding in or out, are worked out.
    std::array<int, waveform::maxHarmonic> harmonics {};
    std::array<double, waveform::maxHarmonic> x {}, y {}, turnX {}, turnY {};
    std::array<float, waveform::maxHarmonic> gains {};
    size_t numHarmonics = 0;

    for (int k = 1; k <= waveform::maxHarmonic; ++k)
    {
        const auto index = (size_t) k;
        const auto playing = cosines[index] != 0.0f || sines[index] != 0.0f;
        const auto gliding = glideSamplesLeft > 0 && (cosineSteps[index] != 0.0f || sineSteps[index] != 0.0f);

        if (! playing && ! gliding)
            continue;

        const auto angle = twoPi * k * phase;
        const auto turn = twoPi * k * cyclesPerSample;

        harmonics[numHarmonics] = k;
        x[numHarmonics] = std::cos (angle);
        y[numHarmonics] = std::sin (angle);
        turnX[numHarmonics] = std::cos (turn);
        turnY[numHarmonics] = std::sin (turn);
        gains[numHarmonics] = bandLimitGain (k * frequency, sampleRate);
        ++numHarmonics;
    }

    for (int s = 0; s < numSamples; ++s)
    {
        auto value = 0.0f;

        for (size_t i = 0; i < numHarmonics; ++i)
        {
            const auto k = (size_t) harmonics[i];
            value += gains[i] * (cosines[k] * (float) x[i] + sines[k] * (float) y[i]);

            const auto nextX = x[i] * turnX[i] - y[i] * turnY[i];
            y[i] = x[i] * turnY[i] + y[i] * turnX[i];
            x[i] = nextX;
        }

        if (glideSamplesLeft > 0)
        {
            if (--glideSamplesLeft == 0)
            {
                cosines = targets.cosines;
                sines = targets.sines;
            }
            else
            {
                for (size_t i = 0; i < numHarmonics; ++i)
                {
                    const auto k = (size_t) harmonics[i];
                    cosines[k] += cosineSteps[k];
                    sines[k] += sineSteps[k];
                }
            }
        }

        const auto sample = value * level * envelope.getNextSample();

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addSample (channel, startSample + s, sample);

        if (! envelope.isActive())
        {
            clearCurrentNote();
            return;
        }
    }

    phase += numSamples * cyclesPerSample;
    phase -= std::floor (phase);
}
