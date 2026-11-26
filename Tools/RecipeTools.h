// What the tools that make presets from recipes share: setting a sound up from Init, a step at a time. A recipe starts
// from Init, the sound EZ Synth starts with, and changes what it needs to.

#pragma once

#include "../Source/SynthProcessor.h"

#include <functional>
#include <optional>
#include <vector>

namespace recipe
{
    using namespace modulation;

    constexpr auto pi = juce::MathConstants<double>::pi;

    inline void setParameter (SynthProcessor& synth, const juce::String& id, float value)
    {
        auto& parameter = *synth.parameters.getParameter (id);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    }

    /** Switches an oscillator on, with a preset wave played with some of its harmonics, at a level. */
    inline void setOscillator (SynthProcessor& synth, int oscillator, waveform::Preset preset, int numHarmonics, float level)
    {
        synth.setShape (oscillator, waveform::makePreset (preset), preset);
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) numHarmonics);
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
    }

    /** Sets the amplitudes of an oscillator's harmonics, from the 1st up. */
    inline void setHarmonics (SynthProcessor& synth, int oscillator, const std::vector<float>& amplitudes)
    {
        auto harmonic = 1;

        for (const auto amplitude : amplitudes)
            synth.setHarmonicAmplitude (oscillator, harmonic++, amplitude);
    }

    inline void setEnvelope (SynthProcessor& synth, int envelope, const EnvelopeSettings& settings)
    {
        setParameter (synth, SynthProcessor::attackId (envelope), settings.attack);
        setParameter (synth, SynthProcessor::decayId (envelope), settings.decay);
        setParameter (synth, SynthProcessor::sustainId (envelope), settings.sustain);
        setParameter (synth, SynthProcessor::releaseId (envelope), settings.release);
    }

    /** A wave drawn by a function of how far through its cycle it is, from 0 to 1. */
    inline waveform::Shape draw (const std::function<double (double)>& valueAt)
    {
        waveform::Shape shape {};

        for (int i = 0; i < waveform::numPoints; ++i)
            shape[(size_t) i] = (float) valueAt ((double) i / waveform::numPoints);

        return shape;
    }

    /** A sawtooth that falls instead of rising. The preset sawtooth rises, so its harmonics are the opposite way up to
        those of a sine, square or triangle, and the oscillators' harmonics would cancel instead of adding up.
    */
    inline waveform::Shape fallingSawtooth()
    {
        auto shape = waveform::makePreset (waveform::Preset::sawtooth);

        for (auto& value : shape)
            value = -value;

        return shape;
    }

    /** Switches an oscillator on, with a drawn wave played with some of its harmonics, at a level. */
    inline void setOscillator (SynthProcessor& synth, int oscillator, const waveform::Shape& shape, int numHarmonics, float level)
    {
        synth.setShape (oscillator, shape);
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) numHarmonics);
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
    }

    inline void assign (SynthProcessor& synth, int target, int source, float range, bool inverted = false, float phase = 0.0f)
    {
        synth.setAssignment (target, { source, range, inverted, phase });
    }

    /** Has a source trigger a control, such as randomizing, each time it crosses a threshold, from 0 to 1. */
    inline void trigger (SynthProcessor& synth, int target, int source, float threshold)
    {
        synth.setAssignment (target, { source, 1.0f, false, 0.0f, threshold });
    }

    inline void assignVelocity (SynthProcessor& synth, int target, float range)
    {
        synth.setVelocityAssignment (target, { modulation::velocitySource, range, false });
    }

    inline void assignModWheel (SynthProcessor& synth, int target, float range, bool inverted = false)
    {
        synth.setModWheelAssignment (target, { modulation::modWheelSource, range, inverted });
    }

    /** Switches an oscillator on with just the harmonics given, from the 1st up, at a level, tuned some semitones
        from the note.
    */
    inline void setPartials (SynthProcessor& synth, int oscillator, float level, float tune, const std::vector<float>& amplitudes)
    {
        synth.setShape (oscillator, {});
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) amplitudes.size());
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
        setParameter (synth, SynthProcessor::tuneId (oscillator), tune);
        setHarmonics (synth, oscillator, amplitudes);
    }

    /** Switches an oscillator on playing white noise, rather than its harmonics, at a level. */
    inline void setNoise (SynthProcessor& synth, int oscillator, float level)
    {
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::noiseId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
    }

    inline void setTune (SynthProcessor& synth, int oscillator, float semitones)
    {
        setParameter (synth, SynthProcessor::tuneId (oscillator), semitones);
    }

    /** Sets how far modulation moves an oscillator's tune: an LFO with a range of R swings it R times this many
        semitones either way, and an envelope raises it by up to twice that.
    */
    inline void setTuneReach (SynthProcessor& synth, int oscillator, double semitones)
    {
        setParameter (synth, SynthProcessor::tuneZoomId (oscillator), (float) (std::log (24.0 / semitones) / std::log (24.0)));
    }

    enum class Glide { off, always, legato };

    /** Glides some oscillators, each as it's set, rather than every oscillator alike. */
    inline void setGlide (SynthProcessor& synth, std::initializer_list<int> oscillators, Glide glide, float seconds)
    {
        setParameter (synth, SynthProcessor::glideAllId, 0.0f);

        for (const auto oscillator : oscillators)
        {
            setParameter (synth, SynthProcessor::glideId (oscillator), (float) glide);
            setParameter (synth, SynthProcessor::glideTimeId (oscillator), seconds);
        }
    }

    /** Switches a filter in, as a type, at a cutoff, with a resonance for a low or high pass. */
    inline void setFilter (SynthProcessor& synth, int filter, FilterType type, float cutoff, float resonance = 0.0f)
    {
        const auto ids = SynthProcessor::filterIds (filter);
        setParameter (synth, ids.on, 1.0f);
        setParameter (synth, ids.type, (float) type);
        setParameter (synth, ids.cutoff, cutoff);
        setParameter (synth, ids.resonance, resonance);
    }

    /** How an LFO runs: looping all the time, once from each key, or forever from each key. */
    constexpr int loops = lfoLoops, once = 1, fromKey = lfoForever;

    inline void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, int mode, const waveform::Shape& shape,
                 std::optional<waveform::Preset> preset = std::nullopt)
    {
        setParameter (synth, SynthProcessor::lfoPeriodId (lfo), periodSeconds);
        setParameter (synth, SynthProcessor::lfoModeId (lfo), (float) mode);
        synth.setLfoShape (lfo, shape, preset);
    }

    inline void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, int mode, waveform::Preset preset)
    {
        setLfo (synth, lfo, periodSeconds, mode, waveform::makePreset (preset), preset);
    }

    /** Has an LFO play a new random wave every cycle, smooth or in steps. */
    inline void setRandom (SynthProcessor& synth, int lfo, bool stepped, int numSteps = 8)
    {
        setParameter (synth, SynthProcessor::lfoRandomId (lfo), 1.0f);
        synth.setDrawing (SynthProcessor::numOscillators + lfo, { stepped, numSteps });
    }

    /** Syncs an LFO to the host's tempo, at a number of cycles a beat, which must be one of the rates there are. */
    inline void setSync (SynthProcessor& synth, int lfo, double cyclesPerBeat)
    {
        setParameter (synth, SynthProcessor::lfoSyncId (lfo), 1.0f);

        for (size_t i = 0; i < beatRates.size(); ++i)
            if (std::abs (beatRates[i] - cyclesPerBeat) < 1.0e-6)
                setParameter (synth, SynthProcessor::lfoBeatRateId (lfo), (float) i);
    }

    /** An LFO's wave, drawn in steps of the values given. */
    inline void setSteps (SynthProcessor& synth, int lfo, std::vector<double> steps)
    {
        synth.setLfoShape (lfo, draw ([&steps] (double t) { return steps[std::min (steps.size() - 1, (size_t) (t * (double) steps.size()))]; }));
        synth.setDrawing (SynthProcessor::numOscillators + lfo, { true, (int) steps.size() });
    }

    /** Has an LFO, run once over some seconds, bring the volume down from a level to nothing along a curve of
        loudness, from 1 at the start, given by a function of the seconds gone. The volume's loudness goes as its
        square, so the LFO draws the square root of the curve.
    */
    inline void setRing (SynthProcessor& synth, int lfo, float seconds, float level, const std::function<double (double)>& loudnessAt)
    {
        setParameter (synth, SynthProcessor::volumeId, level / 2.0f);
        setLfo (synth, lfo, seconds, once, draw ([&] (double t)
        {
            const auto loudness = std::max (0.0, loudnessAt (seconds * t) - loudnessAt (seconds) * t) / loudnessAt (0.0);
            return 2.0 * std::sqrt (loudness) - 1.0;
        }));
        assign (synth, volumeTarget, lfo, level);
    }

    /** Adds a pitch set for a range of keys, sounding as the default does, and makes it the one the recipe sets. */
    inline void addPitchSet (SynthProcessor& synth, int lowest, int highest)
    {
        synth.setActivePitchSet (0);
        synth.setActivePitchSet (synth.addPitchSet ({ lowest, highest }));
    }

    constexpr int lfo1 = 0, lfo2 = 1, lfo3 = 2, lfo4 = 3, lfo5 = 4, lfo6 = 5, lfo7 = 6, lfo8 = 7;
    constexpr int env1 = envelopeSource (0), env2 = envelopeSource (1), env3 = envelopeSource (2), env4 = envelopeSource (3);
    constexpr int osc1 = 0, osc2 = 1, osc3 = 2, osc4 = 3, osc5 = 4, osc6 = 5;
    constexpr auto sine = waveform::Preset::sine, square = waveform::Preset::square, triangle = waveform::Preset::triangle;

    /** Pans an oscillator, from 0, all the way left, to 1, all the way right. */
    inline void setPan (SynthProcessor& synth, int oscillator, float pan)
    {
        setParameter (synth, SynthProcessor::panId (oscillator), pan);
    }

    /** Stacks voices of an oscillator, the outermost detuned so many cents either way, spread from side to side. */
    inline void setUnison (SynthProcessor& synth, int oscillator, int voices, float cents, float spread)
    {
        setParameter (synth, SynthProcessor::unisonVoicesId (oscillator), (float) voices);
        setParameter (synth, SynthProcessor::unisonDetuneId (oscillator), cents);
        setParameter (synth, SynthProcessor::unisonSpreadId (oscillator), spread);
    }

    /** Sends an oscillator through only the filters given, numbered from 0, skipping the rest. */
    inline void setSends (SynthProcessor& synth, int oscillator, std::initializer_list<int> filters)
    {
        for (int filter = 0; filter < numFilters; ++filter)
            setParameter (synth, SynthProcessor::filterSendId (oscillator, filter),
                          std::find (filters.begin(), filters.end(), filter) != filters.end() ? 1.0f : 0.0f);
    }

    /** A recipe: a preset's name, and what makes its sound from Init. */
    struct Recipe
    {
        const char* name;
        std::function<void (SynthProcessor&)> make;
    };
}
