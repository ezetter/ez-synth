#include "HarmonicVoice.h"

#include "Tuning.h"

namespace
{
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;

    /** How loud a note is, leaving headroom for chords. How its velocity changes that is up to what velocity is
        assigned to, which to start with is the volume.
    */
    constexpr float noteLevel = 0.3f;

    /** How quickly a playing note follows changes to its harmonics: it covers about two thirds of the way in this
        time, and nearly all of it in four times as long.
    */
    constexpr double glideSeconds = 0.005;

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

namespace
{
    /** The voice's own envelope, which keeps notes from clicking as they start and stop. */
    const juce::ADSR::Parameters normalEnvelope { 0.008f, 0.05f, 1.0f, HarmonicVoice::releaseSeconds };

    /** Once an envelope moving the loudness has finished its release, the note has already faded, so it just stops. */
    const juce::ADSR::Parameters afterLoudnessEnvelope { 0.008f, 0.05f, 1.0f, 0.03f };
}

HarmonicVoice::HarmonicVoice (const Patch& patchToPlay)
    : patch (patchToPlay)
{
    ownEnvelope.setParameters (normalEnvelope);
}

bool HarmonicVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<HarmonicSound*> (sound) != nullptr;
}

void HarmonicVoice::setCurrentPlaybackSampleRate (double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate <= 0.0)
        return;

    ownEnvelope.setSampleRate (newRate);
    gain.reset (newRate, 0.02);

    for (auto& filter : filters)
        filter.prepare ({ newRate, (juce::uint32) controlInterval, 1 }, false, 1000.0f, 0.0f);
}

void HarmonicVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    noteNumber = midiNoteNumber;
    pitchWheel = currentPitchWheelPosition;
    updateFrequency();

    phases.fill (0.0);
    noteVelocity = velocity;
    samplesSinceNoteOn = 0;

    for (auto& run : envelopeRuns)
        run.start();

    // The first chunk sets the harmonics, filters and loudness outright, with nothing to glide from.
    startingNote = true;
    waitingForEnvelopes = false;

    ownEnvelope.setParameters (normalEnvelope);
    ownEnvelope.noteOn();
}

void HarmonicVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        for (auto& run : envelopeRuns)
            run.release();

        // An envelope moving the loudness decides how the note fades, so the note waits for it to finish.
        if (usesLoudnessEnvelope())
            waitingForEnvelopes = true;
        else
            ownEnvelope.noteOff();
    }
    else
    {
        for (auto& run : envelopeRuns)
            run.stop();

        waitingForEnvelopes = false;
        ownEnvelope.reset();
        clearCurrentNote();
    }
}

bool HarmonicVoice::usesLoudnessEnvelope() const
{
    for (int target = 0; target < modulation::numTargets; ++target)
        if (const auto& assignment = patch.assignments[(size_t) target];
            assignment.isActive() && modulation::isEnvelope (assignment.source) && modulation::isLoudness (target))
            return true;

    return false;
}

bool HarmonicVoice::isLoudnessEnvelopeRunning() const
{
    for (int target = 0; target < modulation::numTargets; ++target)
        if (const auto& assignment = patch.assignments[(size_t) target];
            assignment.isActive() && modulation::isEnvelope (assignment.source) && modulation::isLoudness (target)
            && ! envelopeRuns[(size_t) (assignment.source - modulation::numLfos)].isIdle())
            return true;

    return false;
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

HarmonicVoice::Targets HarmonicVoice::getTargets (int startSample) const
{
    using namespace modulation;

    // How far through its periods each LFO is. A looping LFO carries on from where it was at the start of the block. One
    // that runs once started with the note, as the envelopes did.
    std::array<double, numLfos> lfoPeriodsGone {};

    for (size_t i = 0; i < lfoPeriodsGone.size(); ++i)
    {
        const auto& lfo = patch.lfos[i];
        lfoPeriodsGone[i] = lfo.once ? (double) samplesSinceNoteOn / lfo.periodSamples
                                     : lfo.phase + startSample / lfo.periodSamples;
    }

    // Where the source assigned to a control is for it. A control given a phase follows where its LFO was that much
    // of a period ago.
    const auto sourceValue = [this, &lfoPeriodsGone] (const Assignment& assignment)
    {
        if (isEnvelope (assignment.source))
            return envelopeRuns[(size_t) (assignment.source - numLfos)].getLevel();

        const auto& lfo = patch.lfos[(size_t) assignment.source];
        return lfoValue (lfo.shape, lfoPeriodsGone[(size_t) assignment.source] - assignment.phase, lfo.once);
    };

    // Moves a control's position, from 0 to 1, by the LFO or envelope assigned to it, and by the note's velocity, if
    // either is. An LFO that's switched off moves nothing.
    const auto modulate = [this, &sourceValue] (int target, float position)
    {
        auto offset = 0.0f;

        if (const auto& assignment = patch.assignments[(size_t) target];
            assignment.isActive() && (isEnvelope (assignment.source) || patch.lfos[(size_t) assignment.source].on))
            offset += assignment.offset (sourceValue (assignment));

        if (const auto& byVelocity = patch.velocityAssignments[(size_t) target]; byVelocity.isActive())
            offset += byVelocity.offset (noteVelocity);

        return juce::jlimit (0.0f, 1.0f, position + offset);
    };

    Targets targets;

    for (int i = 0; i < numOscillators; ++i)
    {
        const auto& oscillator = patch.oscillators[(size_t) i];

        // Its tuning is followed even while it's off, so it doesn't slide into tune as it's switched on.
        targets.tunes[(size_t) i] = tuning::octave * modulate (oscillatorTuneTarget (i), oscillator.tune / tuning::octave);

        if (modulate (oscillatorOnTarget (i), oscillator.on ? 1.0f : 0.0f) < 0.5f)
            continue;

        const auto oscillatorLevel = modulate (oscillatorLevelTarget (i), oscillator.level);
        const auto numHarmonics = positionToHarmonics (modulate (oscillatorHarmonicsTarget (i),
                                                                 harmonicsToPosition (oscillator.numHarmonics)));

        // A wave whose harmonics add up to more than full scale is turned down to fit, so it can't swamp the others.
        // An LFO moving a harmonic doesn't change how far, so the wave doesn't pump.
        const auto scale = oscillatorLevel / std::max (1.0f, oscillator.peaks[(size_t) numHarmonics]);

        for (int harmonic = 1; harmonic <= numHarmonics; ++harmonic)
        {
            const auto index = (size_t) harmonic - 1;
            const auto target = harmonicTarget (i, harmonic);
            auto amplitude = oscillator.amplitudes[index];

            if (patch.assignments[(size_t) target].isActive() || patch.velocityAssignments[(size_t) target].isActive())
                amplitude = amplitudeRange().convertFrom0to1 (modulate (target, amplitudeRange().convertTo0to1 (amplitude)));

            // A sin (θ + φ) = A sin φ cos θ + A cos φ sin θ.
            targets.cosines[(size_t) i][(size_t) harmonic] = scale * amplitude * oscillator.phaseSines[index];
            targets.sines[(size_t) i][(size_t) harmonic] = scale * amplitude * oscillator.phaseCosines[index];
        }
    }

    for (int i = 0; i < numFilters; ++i)
    {
        const auto& filter = patch.filters[(size_t) i];
        targets.filters[(size_t) i] = { modulate (filterOnTarget (i), filter.on ? 1.0f : 0.0f) >= 0.5f,
                                        FilterStage::positionToCutoff (modulate (filterCutoffTarget (i),
                                                                                 FilterStage::cutoffToPosition (filter.cutoff))),
                                        modulate (filterResonanceTarget (i), filter.resonance) };
    }

    targets.gain = Patch::volumeToGain (modulate (volumeTarget, patch.volume));
    return targets;
}

void HarmonicVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    while (numSamples > 0 && isVoiceActive())
    {
        const auto chunkSize = std::min (numSamples, controlInterval);
        renderChunk (output, startSample, chunkSize);
        startSample += chunkSize;
        numSamples -= chunkSize;
    }
}

void HarmonicVoice::renderChunk (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const auto sampleRate = getSampleRate();

    // An envelope stage that takes no time, such as an attack of 0, is over before the note's first sound.
    if (startingNote)
        for (size_t i = 0; i < envelopeRuns.size(); ++i)
            envelopeRuns[i].advance (patch.envelopes[i], 0.0);

    const auto targets = getTargets (startSample);

    if (startingNote)
    {
        cosines = targets.cosines;
        sines = targets.sines;
        std::copy (targets.tunes.begin(), targets.tunes.end(), tunes.begin());
        gain.setCurrentAndTargetValue (targets.gain);

        for (size_t i = 0; i < filters.size(); ++i)
            filters[i].jumpTo (targets.filters[i].on, targets.filters[i].cutoff, targets.filters[i].resonance);

        startingNote = false;
    }

    gain.setTargetValue (targets.gain);

    for (size_t i = 0; i < filters.size(); ++i)
        filters[i].setTargets (targets.filters[i].on, targets.filters[i].cutoff, targets.filters[i].resonance);

    // Each chunk moves the harmonics and tunings its share of the way to their targets, so they follow smoothly
    // however the targets move. A tuning holds steady through the chunk.
    const auto glideSamples = std::max ((double) numSamples, glideSeconds * sampleRate);
    const auto glideShare = numSamples / glideSamples;
    std::array<double, modulation::numOscillators> cyclesPerSample {};

    for (size_t i = 0; i < tunes.size(); ++i)
    {
        tunes[i] += (targets.tunes[i] - tunes[i]) * glideShare;

        if (std::abs (targets.tunes[i] - tunes[i]) < 1.0e-4)
            tunes[i] = targets.tunes[i];

        cyclesPerSample[i] = frequency * tuning::toRatio (tunes[i]) / sampleRate;
    }

    // Each harmonic is a point going round a circle, turned by a fixed angle every sample, which is much cheaper
    // than calling sin() every sample. The points are placed afresh from each oscillator's phase each chunk, so
    // rounding errors can't build up. Only the harmonics that are playing, or gliding in or out, are worked out.
    struct Playing
    {
        float cosine = 0.0f, sine = 0.0f, cosineStep = 0.0f, sineStep = 0.0f, bandLimit = 0.0f;
        double x = 0.0, y = 0.0, turnX = 0.0, turnY = 0.0;
        size_t oscillator = 0, harmonic = 1;
    };

    std::array<Playing, (size_t) (modulation::numOscillators * waveform::maxHarmonics)> playing;
    size_t numPlaying = 0;

    for (size_t i = 0; i < cosines.size(); ++i)
    {
        for (size_t k = 1; k < cosines[i].size(); ++k)
        {
            auto& cosine = cosines[i][k];
            auto& sine = sines[i][k];
            const auto targetCosine = targets.cosines[i][k];
            const auto targetSine = targets.sines[i][k];

            // Gliding this way never quite arrives, so a harmonic that's nearly there is put there, and one fading
            // out stops being worked out once it's gone.
            if (std::abs (targetCosine - cosine) < 1.0e-6f && std::abs (targetSine - sine) < 1.0e-6f)
            {
                cosine = targetCosine;
                sine = targetSine;

                if (cosine == 0.0f && sine == 0.0f)
                    continue;
            }

            const auto angle = twoPi * (double) k * phases[i];
            const auto turn = twoPi * (double) k * cyclesPerSample[i];

            auto& harmonic = playing[numPlaying++];
            harmonic.cosine = cosine;
            harmonic.sine = sine;
            harmonic.cosineStep = (float) ((targetCosine - cosine) / glideSamples);
            harmonic.sineStep = (float) ((targetSine - sine) / glideSamples);
            harmonic.bandLimit = bandLimitGain ((double) k * cyclesPerSample[i] * sampleRate, sampleRate);
            harmonic.x = std::cos (angle);
            harmonic.y = std::sin (angle);
            harmonic.turnX = std::cos (turn);
            harmonic.turnY = std::sin (turn);
            harmonic.oscillator = i;
            harmonic.harmonic = k;
        }
    }

    auto& lowPass = filters[(size_t) modulation::lowPass];
    auto& highPass = filters[(size_t) modulation::highPass];
    const auto lowPassActive = lowPass.isActive();
    const auto highPassActive = highPass.isActive();

    // A filter that's out starts afresh when it's switched back in.
    if (! lowPassActive)
        lowPass.reset();

    if (! highPassActive)
        highPass.reset();

    for (int s = 0; s < numSamples; ++s)
    {
        auto value = 0.0f;

        for (size_t i = 0; i < numPlaying; ++i)
        {
            auto& harmonic = playing[i];
            value += harmonic.bandLimit * (harmonic.cosine * (float) harmonic.x + harmonic.sine * (float) harmonic.y);

            const auto nextX = harmonic.x * harmonic.turnX - harmonic.y * harmonic.turnY;
            harmonic.y = harmonic.x * harmonic.turnY + harmonic.y * harmonic.turnX;
            harmonic.x = nextX;

            harmonic.cosine += harmonic.cosineStep;
            harmonic.sine += harmonic.sineStep;
        }

        if (highPassActive)
        {
            highPass.advance();
            value = highPass.process (0, value);
        }

        if (lowPassActive)
        {
            lowPass.advance();
            value = lowPass.process (0, value);
        }

        const auto sample = value * noteLevel * gain.getNextValue() * ownEnvelope.getNextSample();

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.addSample (channel, startSample + s, sample);

        if (! ownEnvelope.isActive())
        {
            clearCurrentNote();
            return;
        }
    }

    for (size_t i = 0; i < numPlaying; ++i)
    {
        cosines[playing[i].oscillator][playing[i].harmonic] = playing[i].cosine;
        sines[playing[i].oscillator][playing[i].harmonic] = playing[i].sine;
    }

    for (size_t i = 0; i < phases.size(); ++i)
    {
        phases[i] += numSamples * cyclesPerSample[i];
        phases[i] -= std::floor (phases[i]);
    }

    samplesSinceNoteOn += numSamples;

    for (size_t i = 0; i < envelopeRuns.size(); ++i)
        envelopeRuns[i].advance (patch.envelopes[i], numSamples / sampleRate);

    // Once every envelope moving the loudness has finished its release, the note ends.
    if (waitingForEnvelopes && ! isLoudnessEnvelopeRunning())
    {
        waitingForEnvelopes = false;
        ownEnvelope.setParameters (afterLoudnessEnvelope);
        ownEnvelope.noteOff();
    }
}
