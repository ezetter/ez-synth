// Makes the factory presets that come with EZ Synth, from the recipes below, and saves them in a folder: Presets, in
// the current folder, unless another is named. Run it from the repository's folder after changing a recipe, then
// build again, to build the new presets into EZ Synth:
//
//     ./build/EZSynthMakeFactoryPresets
//
// Each recipe starts from Init, the sound EZ Synth starts with, and changes what it needs to.

#include "../Source/SynthProcessor.h"

#include <iostream>

namespace
{
    using namespace modulation;

    constexpr auto pi = juce::MathConstants<double>::pi;

    void setParameter (SynthProcessor& synth, const juce::String& id, float value)
    {
        auto& parameter = *synth.parameters.getParameter (id);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    }

    /** Switches an oscillator on, with a preset wave played with some of its harmonics, at a level. */
    void setOscillator (SynthProcessor& synth, int oscillator, waveform::Preset preset, int numHarmonics, float level)
    {
        synth.setShape (oscillator, waveform::makePreset (preset), preset);
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) numHarmonics);
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
    }

    /** Sets the amplitudes of an oscillator's harmonics, from the 1st up. */
    void setHarmonics (SynthProcessor& synth, int oscillator, const std::vector<float>& amplitudes)
    {
        auto harmonic = 1;

        for (const auto amplitude : amplitudes)
            synth.setHarmonicAmplitude (oscillator, harmonic++, amplitude);
    }

    void setEnvelope (SynthProcessor& synth, int envelope, const EnvelopeSettings& settings)
    {
        setParameter (synth, SynthProcessor::attackId (envelope), settings.attack);
        setParameter (synth, SynthProcessor::decayId (envelope), settings.decay);
        setParameter (synth, SynthProcessor::sustainId (envelope), settings.sustain);
        setParameter (synth, SynthProcessor::releaseId (envelope), settings.release);
    }

    /** A wave drawn by a function of how far through its cycle it is, from 0 to 1. */
    waveform::Shape draw (const std::function<double (double)>& valueAt)
    {
        waveform::Shape shape {};

        for (int i = 0; i < waveform::numPoints; ++i)
            shape[(size_t) i] = (float) valueAt ((double) i / waveform::numPoints);

        return shape;
    }

    /** A sawtooth that falls instead of rising. The preset sawtooth rises, so its harmonics are the opposite way up to
        those of a sine, square or triangle, and the oscillators' harmonics would cancel instead of adding up.
    */
    waveform::Shape fallingSawtooth()
    {
        auto shape = waveform::makePreset (waveform::Preset::sawtooth);

        for (auto& value : shape)
            value = -value;

        return shape;
    }

    /** Switches an oscillator on, with a drawn wave played with some of its harmonics, at a level. */
    void setOscillator (SynthProcessor& synth, int oscillator, const waveform::Shape& shape, int numHarmonics, float level)
    {
        synth.setShape (oscillator, shape);
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) numHarmonics);
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
    }

    void assign (SynthProcessor& synth, int target, int source, float range, bool inverted = false, float phase = 0.0f)
    {
        synth.setAssignment (target, { source, range, inverted, phase });
    }

    /** Has a source trigger a control, such as randomizing, each time it crosses a threshold, from 0 to 1. */
    void trigger (SynthProcessor& synth, int target, int source, float threshold)
    {
        synth.setAssignment (target, { source, 1.0f, false, 0.0f, threshold });
    }

    void assignVelocity (SynthProcessor& synth, int target, float range)
    {
        synth.setVelocityAssignment (target, { modulation::velocitySource, range, false });
    }

    void assignModWheel (SynthProcessor& synth, int target, float range, bool inverted = false)
    {
        synth.setModWheelAssignment (target, { modulation::modWheelSource, range, inverted });
    }

    /** Switches an oscillator on with just the harmonics given, from the 1st up, at a level, tuned some semitones
        from the note.
    */
    void setPartials (SynthProcessor& synth, int oscillator, float level, float tune, const std::vector<float>& amplitudes)
    {
        synth.setShape (oscillator, {});
        setParameter (synth, SynthProcessor::onId (oscillator), 1.0f);
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) amplitudes.size());
        setParameter (synth, SynthProcessor::levelId (oscillator), level);
        setParameter (synth, SynthProcessor::tuneId (oscillator), tune);
        setHarmonics (synth, oscillator, amplitudes);
    }

    void setTune (SynthProcessor& synth, int oscillator, float semitones)
    {
        setParameter (synth, SynthProcessor::tuneId (oscillator), semitones);
    }

    /** Sets how far modulation moves an oscillator's tune: an LFO with a range of R swings it R times this many
        semitones either way, and an envelope raises it by up to twice that.
    */
    void setTuneReach (SynthProcessor& synth, int oscillator, double semitones)
    {
        setParameter (synth, SynthProcessor::tuneZoomId (oscillator), (float) (std::log (24.0 / semitones) / std::log (24.0)));
    }

    enum class Glide { off, always, legato };

    void setGlide (SynthProcessor& synth, std::initializer_list<int> oscillators, Glide glide, float seconds)
    {
        for (const auto oscillator : oscillators)
        {
            setParameter (synth, SynthProcessor::glideId (oscillator), (float) glide);
            setParameter (synth, SynthProcessor::glideTimeId (oscillator), seconds);
        }
    }

    /** Switches a filter in, as a type, at a cutoff, with a resonance for a low or high pass. */
    void setFilter (SynthProcessor& synth, int filter, FilterType type, float cutoff, float resonance = 0.0f)
    {
        const auto ids = SynthProcessor::filterIds (filter);
        setParameter (synth, ids.on, 1.0f);
        setParameter (synth, ids.type, (float) type);
        setParameter (synth, ids.cutoff, cutoff);
        setParameter (synth, ids.resonance, resonance);
    }

    /** How an LFO runs: looping all the time, once from each key, or forever from each key. */
    constexpr int loops = lfoLoops, once = 1, fromKey = lfoForever;

    void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, int mode, const waveform::Shape& shape,
                 std::optional<waveform::Preset> preset = std::nullopt)
    {
        setParameter (synth, SynthProcessor::lfoPeriodId (lfo), periodSeconds);
        setParameter (synth, SynthProcessor::lfoModeId (lfo), (float) mode);
        synth.setLfoShape (lfo, shape, preset);
    }

    void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, int mode, waveform::Preset preset)
    {
        setLfo (synth, lfo, periodSeconds, mode, waveform::makePreset (preset), preset);
    }

    /** Has an LFO play a new random wave every cycle, smooth or in steps. */
    void setRandom (SynthProcessor& synth, int lfo, bool stepped, int numSteps = 8)
    {
        setParameter (synth, SynthProcessor::lfoRandomId (lfo), 1.0f);
        synth.setDrawing (SynthProcessor::numOscillators + lfo, { stepped, numSteps });
    }

    /** Syncs an LFO to the host's tempo, at a number of cycles a beat, which must be one of the rates there are. */
    void setSync (SynthProcessor& synth, int lfo, double cyclesPerBeat)
    {
        setParameter (synth, SynthProcessor::lfoSyncId (lfo), 1.0f);

        for (size_t i = 0; i < beatRates.size(); ++i)
            if (std::abs (beatRates[i] - cyclesPerBeat) < 1.0e-6)
                setParameter (synth, SynthProcessor::lfoBeatRateId (lfo), (float) i);
    }

    /** An LFO's wave, drawn in steps of the values given. */
    void setSteps (SynthProcessor& synth, int lfo, std::vector<double> steps)
    {
        synth.setLfoShape (lfo, draw ([&steps] (double t) { return steps[std::min (steps.size() - 1, (size_t) (t * (double) steps.size()))]; }));
        synth.setDrawing (SynthProcessor::numOscillators + lfo, { true, (int) steps.size() });
    }

    /** Has an LFO, run once over some seconds, bring the volume down from a level to nothing along a curve of
        loudness, from 1 at the start, given by a function of the seconds gone. The volume's loudness goes as its
        square, so the LFO draws the square root of the curve.
    */
    void setRing (SynthProcessor& synth, int lfo, float seconds, float level, const std::function<double (double)>& loudnessAt)
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
    void addPitchSet (SynthProcessor& synth, int lowest, int highest)
    {
        synth.setActivePitchSet (0);
        synth.setActivePitchSet (synth.addPitchSet ({ lowest, highest }));
    }

    constexpr int lfo1 = 0, lfo2 = 1, lfo3 = 2;
    constexpr int env1 = envelopeSource (0), env2 = envelopeSource (1), env3 = envelopeSource (2), env4 = envelopeSource (3);
    constexpr int osc1 = 0, osc2 = 1, osc3 = 2, osc4 = 3, osc5 = 4, osc6 = 5;
    constexpr auto sine = waveform::Preset::sine, square = waveform::Preset::square, triangle = waveform::Preset::triangle;

    struct Recipe
    {
        const char* name;
        std::function<void (SynthProcessor&)> make;
    };

    //==============================================================================
    // The Grand Piano's three registers, each a pitch set, made the same way with different strings.
    struct PianoRegister
    {
        std::vector<float> string, secondString, strike;
        float stretch;          // how sharp the upper harmonics are, as a stiff string's: harmonic k by this times k cubed
        float cutoff;           // where the felt's low pass rests, in hertz
        float seconds;          // how long the ring's LFO runs
        double quick, slow;     // the two stages of the ring: seconds for each to fall to about a third
        float level;
    };

    void makePiano (SynthProcessor& synth, const PianoRegister& piano)
    {
        // The first string: its harmonics as the hammer leaves them, each a little sharper than the last, as a stiff
        // string's are.
        setPartials (synth, osc1, 1.0f, 0.0f, piano.string);

        // The second string, 2 cents sharp, quieter, beating slowly against the first.
        setPartials (synth, osc3, 0.25f, 0.02f, piano.secondString);

        // The strike: bright upper harmonics, a little sharp, gone within a second.
        setPartials (synth, osc2, 0.0f, 0.03f, piano.strike);
        setEnvelope (synth, 0, { 0.002f, 0.9f, 0.0f, 0.25f });
        assign (synth, oscillatorLevelTarget (osc2), env1, 0.7f);

        for (const auto oscillator : { osc1, osc2 })
            for (int harmonic = 2; harmonic <= waveform::maxHarmonics; ++harmonic)
                synth.setHarmonicTune (oscillator, harmonic, std::min (0.6f, piano.stretch * (float) (harmonic * harmonic * harmonic)));

        // Softer notes strike more quietly, and less brightly. Velocity turns down the strings' levels, rather
        // than the volume, so a soft note rings as long as a hard one, just more quietly.
        assignVelocity (synth, volumeTarget, 0.15f);
        assignVelocity (synth, oscillatorLevelTarget (osc1), 0.85f);
        assignVelocity (synth, oscillatorLevelTarget (osc3), 0.25f);
        assignVelocity (synth, oscillatorLevelTarget (osc2), 0.6f);

        // The felt and the wood: a low pass that opens with the strike and closes as the note rings, more for
        // harder notes, so the upper harmonics die away first.
        setFilter (synth, 0, FilterType::lowPass, piano.cutoff);
        setEnvelope (synth, 1, { 0.0f, 2.0f, 0.15f, 0.3f });
        assign (synth, filterCutoffTarget (0), env2, 0.42f);
        assignVelocity (synth, filterCutoffTarget (0), 0.45f);

        // The ring, drawn on an LFO that runs once: a quick fall, then a long, slow one, as a piano's strings fall
        // once their quick first sound has gone, and to nothing at the end. It peaks low enough to leave room for
        // big chords.
        setRing (synth, lfo1, piano.seconds, piano.level, [&piano] (double seconds)
        {
            return 0.55 * std::exp (-seconds / piano.quick) + 0.45 * std::exp (-seconds / piano.slow);
        });
    }

    //==============================================================================
    const std::vector<Recipe> recipes {
        // A sawtooth and a sub through a resonant low pass that an envelope snaps open with each note, further for
        // harder ones. Notes played legato slide into each other, and the mod wheel opens the filter.
        { "Acid Bass", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 1.0f);
            setOscillator (synth, osc2, sine, 1, 0.5f);
            setTune (synth, osc2, -12.0f);
            setGlide (synth, { osc1, osc2 }, Glide::legato, 0.06f);

            setFilter (synth, 0, FilterType::lowPass, 180.0f, 0.72f);
            setEnvelope (synth, 0, { 0.0f, 0.25f, 0.1f, 0.15f });
            assign (synth, filterCutoffTarget (0), env1, 0.45f);
            assignVelocity (synth, filterCutoffTarget (0), 0.25f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 1, { 0.002f, 0.3f, 0.75f, 0.08f });
            assign (synth, volumeTarget, env2, 0.6f);
        } },

        // Two sawtooths a few cents apart, bowed in by an envelope, between a resonant high pass and low pass that
        // stand for the body. A vibrato starts with each note, and legato notes slide.
        { "Bowed Cello", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.8f);
            setOscillator (synth, osc2, fallingSawtooth(), 12, 0.3f);
            setTune (synth, osc2, 0.05f);
            setGlide (synth, { osc1, osc2 }, Glide::legato, 0.08f);

            setFilter (synth, 1, FilterType::highPass, 180.0f, 0.4f);
            setFilter (synth, 0, FilterType::lowPass, 2400.0f, 0.3f);
            assignVelocity (synth, filterCutoffTarget (0), 0.2f);

            setLfo (synth, lfo1, 1.0f / 5.5f, fromKey, sine);

            for (const auto oscillator : { osc1, osc2 })
            {
                setTuneReach (synth, oscillator, 1.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo1, 0.1f);
            }

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.12f, 0.3f, 0.85f, 0.25f });
            assign (synth, volumeTarget, env1, 0.7f);
        } },

        // Three sawtooths spread a few cents apart, through a low pass an envelope opens with each note, further for
        // harder ones. Each note scoops up into its pitch as it starts.
        { "Brass Section", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.7f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.5f);
            setOscillator (synth, osc3, fallingSawtooth(), 16, 0.5f);
            setTune (synth, osc2, 0.08f);
            setTune (synth, osc3, -0.07f);

            setFilter (synth, 0, FilterType::lowPass, 420.0f, 0.2f);
            setEnvelope (synth, 0, { 0.07f, 0.45f, 0.5f, 0.3f });
            assign (synth, filterCutoffTarget (0), env1, 0.4f);
            assignVelocity (synth, filterCutoffTarget (0), 0.25f);

            // The scoop: an envelope that's gone in a moment holds each tune down, a third of a semitone at first.
            setEnvelope (synth, 2, { 0.0f, 0.09f, 0.0f, 0.09f });

            for (const auto oscillator : { osc1, osc2, osc3 })
            {
                setTuneReach (synth, oscillator, 2.0);
                assign (synth, oscillatorTuneTarget (oscillator), env3, 0.09f, true);
            }

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 1, { 0.04f, 0.2f, 0.9f, 0.2f });
            assign (synth, volumeTarget, env2, 0.72f);
        } },

        // A bell's partials, which aren't harmonics of its note: the hum an octave below, the prime, the minor third
        // above it, the fifth and the octaves, some tuned between harmonics. Each group rings for its own time, the
        // high ones dying first, and the strike's clang is gone in a moment. It rings on after the key is let go.
        { "Church Bell", [] (SynthProcessor& synth)
        {
            // An octave down: the hum, the prime and the fifth, which ring longest.
            setPartials (synth, osc1, 0.0f, -12.0f, { 0.5f, 0.9f, 0.2f });
            setEnvelope (synth, 0, { 0.0f, 10.0f, 0.0f, 5.0f });
            assign (synth, oscillatorLevelTarget (osc1), env1, 1.0f);

            // The tierce, a minor third above the prime, which gives a bell its sad sound.
            setPartials (synth, osc2, 0.0f, 3.16f, { 1.0f });
            setEnvelope (synth, 1, { 0.0f, 5.0f, 0.0f, 3.0f });
            assign (synth, oscillatorLevelTarget (osc2), env2, 0.55f);

            // The upper partials, from the same octave down: twice the prime, 2.67, 3, 4, 5.33 and 6.67 times it.
            setPartials (synth, osc3, 0.0f, -12.0f, { 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.35f, 0.0f, 0.3f, 0.0f, 0.0f, 0.15f, 0.0f, 0.1f });
            synth.setHarmonicTune (osc3, 5, 0.33f);
            synth.setHarmonicTune (osc3, 11, -0.33f);
            synth.setHarmonicTune (osc3, 13, 0.33f);
            setEnvelope (synth, 2, { 0.0f, 2.5f, 0.0f, 2.0f });
            assign (synth, oscillatorLevelTarget (osc3), env3, 0.8f);

            // The clang of the clapper, two octaves up and out of tune.
            setPartials (synth, osc4, 0.0f, 24.0f, { 0.0f, 0.5f, 0.4f });
            synth.setHarmonicTune (osc4, 2, 0.3f);
            synth.setHarmonicTune (osc4, 3, -0.2f);
            setEnvelope (synth, 3, { 0.0f, 0.25f, 0.0f, 0.25f });
            assign (synth, oscillatorLevelTarget (osc4), env4, 0.4f);

            setRing (synth, lfo1, 10.0f, 0.6f, [] (double seconds) { return 0.4 * std::exp (-seconds / 0.8) + 0.6 * std::exp (-seconds / 4.0); });
        } },

        // A tonewheel organ's drawbars as one oscillator's harmonics, an octave down, with a percussive third harmonic
        // and a key click on other oscillators. An LFO turns the rotary speaker, moving the volume, the tune and a
        // low pass together, and the mod wheel speeds it from slow to fast. Every note is as loud, as an organ's are.
        { "Drawbar Organ", [] (SynthProcessor& synth)
        {
            // The drawbars, from 16' to 1': the note's sub-octave is the 1st harmonic, the note the 2nd.
            setPartials (synth, osc1, 0.9f, -12.0f, { 1.0f, 1.0f, 0.9f, 0.5f, 0.0f, 0.3f, 0.0f, 0.25f, 0.0f, 0.1f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f });
            synth.setVelocityAssignment (volumeTarget, {});
            setParameter (synth, SynthProcessor::volumeId, 0.62f);

            // The percussion, a twelfth up, gone in a third of a second.
            setPartials (synth, osc2, 0.0f, 19.0f, { 1.0f });
            setEnvelope (synth, 0, { 0.0f, 0.3f, 0.0f, 0.1f });
            assign (synth, oscillatorLevelTarget (osc2), env1, 0.5f);

            // The click as the key goes down.
            setPartials (synth, osc3, 0.0f, 24.0f, { 0.6f, 0.5f, 0.4f, 0.3f });
            setEnvelope (synth, 1, { 0.0f, 0.02f, 0.0f, 0.02f });
            assign (synth, oscillatorLevelTarget (osc3), env2, 0.35f);

            // The rotary speaker.
            setFilter (synth, 0, FilterType::lowPass, 3200.0f, 0.1f);
            setLfo (synth, lfo1, 1.0f / 0.8f, loops, sine);
            assign (synth, volumeTarget, lfo1, 0.07f);
            assign (synth, filterCutoffTarget (0), lfo1, 0.1f, false, 0.25f);
            setTuneReach (synth, osc1, 1.0);
            assign (synth, oscillatorTuneTarget (osc1), lfo1, 0.06f, false, 0.25f);
            assignModWheel (synth, lfoRateTarget (lfo1), 0.31f);
        } },

        // An electric piano's tine and tone bar: a mellow fundamental, a bark that only hard notes bring out, and a
        // bell two octaves up that's gone in a moment. The mod wheel brings out the bark. The low keys ring longer
        // and growl more, and the high keys are shorter and clearer, each in a pitch set of its own.
        { "Electric Piano", [] (SynthProcessor& synth)
        {
            const auto make = [&synth] (float seconds, float bark, float bell, float cutoff)
            {
                setPartials (synth, osc1, 1.0f, 0.0f, { 1.0f, 0.25f, 0.08f });

                setOscillator (synth, osc2, triangle, 5, bark);
                assignVelocity (synth, oscillatorLevelTarget (osc2), bark);
                assignModWheel (synth, oscillatorLevelTarget (osc2), 0.4f);

                // The tine's ping: two octaves up, with a partial that isn't a harmonic.
                setPartials (synth, osc3, 0.0f, 24.0f, { 1.0f, 0.3f });
                synth.setHarmonicTune (osc3, 2, 0.3f);
                setEnvelope (synth, 1, { 0.0f, 0.35f, 0.0f, 0.25f });
                assign (synth, oscillatorLevelTarget (osc3), env2, bell);
                assignVelocity (synth, oscillatorLevelTarget (osc3), bell);

                setFilter (synth, 0, FilterType::lowPass, cutoff, 0.1f);
                assignVelocity (synth, filterCutoffTarget (0), 0.35f);

                setParameter (synth, SynthProcessor::volumeId, 0.0f);
                setEnvelope (synth, 0, { 0.003f, seconds, 0.15f, 0.4f });
                assign (synth, volumeTarget, env1, 0.65f);
            };

            make (3.5f, 0.45f, 0.35f, 3500.0f);

            addPitchSet (synth, 0, 47);
            make (5.5f, 0.6f, 0.2f, 2600.0f);

            addPitchSet (synth, 72, 127);
            make (2.0f, 0.3f, 0.3f, 5000.0f);
            synth.setActivePitchSet (0);
        } },

        // A sawtooth with another a fifth above it and a square an octave below, sliding from each note to the next,
        // with a vibrato. The mod wheel opens the filter.
        { "Fifth Lead", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.8f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.5f);
            setTune (synth, osc2, 7.02f);
            setOscillator (synth, osc3, square, 9, 0.35f);
            setTune (synth, osc3, -12.0f);
            setGlide (synth, { osc1, osc2, osc3 }, Glide::always, 0.07f);
            setParameter (synth, SynthProcessor::volumeId, 0.6f);

            setFilter (synth, 0, FilterType::lowPass, 1500.0f, 0.3f);
            setEnvelope (synth, 0, { 0.0f, 0.4f, 0.4f, 0.2f });
            assign (synth, filterCutoffTarget (0), env1, 0.25f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f);

            setLfo (synth, lfo1, 1.0f / 5.8f, loops, sine);

            for (const auto oscillator : { osc1, osc2, osc3 })
            {
                setTuneReach (synth, oscillator, 1.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo1, 0.09f);
            }
        } },

        // A flute's few harmonics, blown in by an envelope, with a gentle vibrato. Its breath is an oscillator of high
        // harmonics whose tune a fast random LFO smears into noise, loudest as the note starts.
        { "Flute", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 1.0f, 0.0f, { 1.0f, 0.35f, 0.12f, 0.05f, 0.02f });

            // The breath.
            setPartials (synth, osc2, 0.06f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.6f, 0.7f, 0.7f, 0.6f, 0.5f, 0.4f });
            setLfo (synth, lfo2, 1.0f / 90.0f, loops, sine);
            setRandom (synth, lfo2, false);
            assign (synth, oscillatorTuneTarget (osc2), lfo2, 0.06f);
            setEnvelope (synth, 1, { 0.0f, 0.08f, 0.0f, 0.05f });
            assign (synth, oscillatorLevelTarget (osc2), env2, 0.3f);

            setLfo (synth, lfo1, 1.0f / 4.8f, loops, sine);
            setTuneReach (synth, osc1, 1.0);
            assign (synth, oscillatorTuneTarget (osc1), lfo1, 0.06f);
            assign (synth, volumeTarget, lfo1, 0.05f, false, 0.25f);

            setFilter (synth, 0, FilterType::lowPass, 4000.0f);
            assignVelocity (synth, filterCutoffTarget (0), 0.2f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.06f, 0.2f, 0.85f, 0.15f });
            assign (synth, volumeTarget, env1, 0.75f);
        } },

        // Six sine waves stacked up from the note: an octave, a twelfth and two octaves above it, with one a little
        // sharp and one a partial that isn't a harmonic. A slow random LFO for each moves its level, so the stack
        // shimmers, never the same twice. The mod wheel opens it up.
        { "Glass Pad", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.7f, 0.0f, { 1.0f, 0.0f, 0.2f });
            setPartials (synth, osc2, 0.35f, 12.0f, { 1.0f });
            setPartials (synth, osc3, 0.3f, 19.0f, { 1.0f });
            setPartials (synth, osc4, 0.3f, 24.0f, { 1.0f });
            setPartials (synth, osc5, 0.3f, 12.07f, { 1.0f });
            setPartials (synth, osc6, 0.2f, 24.0f, { 0.0f, 1.0f });
            synth.setHarmonicTune (osc6, 2, 0.26f);

            constexpr std::array periods { 3.1f, 4.3f, 5.7f, 7.3f, 6.1f };

            for (int i = 0; i < 5; ++i)
            {
                setLfo (synth, i, periods[(size_t) i], loops, sine);
                setRandom (synth, i, false);
                assign (synth, oscillatorLevelTarget (osc2 + i), i, 0.55f);
            }

            setFilter (synth, 1, FilterType::highPass, 200.0f);
            setFilter (synth, 0, FilterType::lowPass, 2500.0f);
            assignModWheel (synth, filterCutoffTarget (0), 0.35f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 1.2f, 1.0f, 0.85f, 3.0f });
            assign (synth, volumeTarget, env1, 0.7f);
        } },

        // A grand piano. A hammer strikes the strings, bright at first, mellowing as they ring, then dying away in two
        // stages, a quick fall and then a long, slow one, as a piano's strings do. Two strings a hair apart beat gently
        // against each other, and letting go of the key damps them. Softer notes are quieter, and darker too. The
        // bass, the middle and the treble are each a pitch set: the bass strings are rich in harmonics, each a little
        // sharp, as stiff strings' are, and ring longest; the treble's are plain and short.
        { "Grand Piano", [] (SynthProcessor& synth)
        {
            // The middle: the harmonics of a piano's middle register, struck near an eighth of the string's length,
            // which leaves its 8th harmonic weak.
            makePiano (synth, { { 1.0f, 0.62f, 0.36f, 0.24f, 0.14f, 0.09f, 0.05f, 0.012f, 0.03f },
                                { 1.0f, 0.55f, 0.3f, 0.18f, 0.1f, 0.06f },
                                { 0.0f, 0.35f, 0.45f, 0.5f, 0.45f, 0.4f, 0.3f, 0.08f, 0.25f },
                                0.0f, 900.0f, 10.0f, 0.6, 3.5, 0.64f });

            // The bass, up to the B below the C below middle C.
            addPitchSet (synth, 0, 47);
            makePiano (synth, { { 1.0f, 0.85f, 0.7f, 0.55f, 0.45f, 0.36f, 0.28f, 0.08f, 0.18f, 0.14f, 0.11f, 0.09f, 0.07f, 0.05f, 0.04f, 0.03f },
                                { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f, 0.12f, 0.08f },
                                { 0.0f, 0.3f, 0.4f, 0.5f, 0.5f, 0.45f, 0.4f, 0.1f, 0.35f, 0.3f, 0.25f, 0.2f, 0.15f, 0.12f, 0.1f, 0.08f },
                                0.00012f, 650.0f, 10.0f, 1.0, 6.0, 0.66f });

            // The treble, from the C two octaves above middle C.
            addPitchSet (synth, 84, 127);
            makePiano (synth, { { 1.0f, 0.4f, 0.12f, 0.05f },
                                { 1.0f, 0.3f, 0.08f },
                                { 0.0f, 0.5f, 0.4f, 0.25f },
                                0.0f, 2500.0f, 5.0f, 0.3, 1.4, 0.62f });
            synth.setActivePitchSet (0);
        } },

        // A drum: a sine whose pitch an envelope drops two octaves in a tenth of a second, with a click on top. It
        // plays an octave below the keys.
        { "Kick Drum", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 1.0f, -12.0f, { 1.0f });
            setEnvelope (synth, 0, { 0.0f, 0.09f, 0.0f, 0.09f });
            assign (synth, oscillatorTuneTarget (osc1), env1, 0.5f);

            setOscillator (synth, osc2, square, 9, 0.0f);
            setTune (synth, osc2, 24.0f);
            setEnvelope (synth, 2, { 0.0f, 0.012f, 0.0f, 0.012f });
            assign (synth, oscillatorLevelTarget (osc2), env3, 0.4f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 1, { 0.0f, 0.4f, 0.0f, 0.25f });
            assign (synth, volumeTarget, env2, 0.95f);
        } },

        // A wooden bar: its note, and the bar's own partials near 4 and 9.2 times it, which aren't harmonics, each
        // gone sooner than the last. The low bars, a pitch set, ring longer under softer mallets, and the high ones,
        // another, are short and bright.
        { "Marimba", [] (SynthProcessor& synth)
        {
            const auto make = [&synth] (float seconds, float second, float third)
            {
                setPartials (synth, osc1, 1.0f, 0.0f, { 1.0f });

                // 3.9 times the note: nearly two octaves up.
                setPartials (synth, osc2, 0.0f, 23.56f, { 1.0f });
                setEnvelope (synth, 1, { 0.0f, seconds * 0.22f, 0.0f, 0.2f });
                assign (synth, oscillatorLevelTarget (osc2), env2, second);
                assignVelocity (synth, oscillatorLevelTarget (osc2), second * 0.6f);

                // 9.2 times it: the 2nd harmonic of two octaves up, tuned up to 2.3.
                setPartials (synth, osc3, 0.0f, 24.0f, { 0.0f, 1.0f });
                synth.setHarmonicTune (osc3, 2, 0.3f);
                setEnvelope (synth, 2, { 0.0f, seconds * 0.07f, 0.0f, 0.08f });
                assign (synth, oscillatorLevelTarget (osc3), env3, third);
                assignVelocity (synth, oscillatorLevelTarget (osc3), third * 0.8f);

                setParameter (synth, SynthProcessor::volumeId, 0.0f);
                setEnvelope (synth, 0, { 0.0f, seconds, 0.0f, seconds * 0.6f });
                assign (synth, volumeTarget, env1, 0.76f);
            };

            make (1.2f, 0.4f, 0.25f);

            addPitchSet (synth, 0, 59);
            make (2.0f, 0.3f, 0.12f);

            addPitchSet (synth, 84, 127);
            make (0.5f, 0.45f, 0.3f);
            synth.setActivePitchSet (0);
        } },

        // A music box's comb: each tooth an octave above its key, with a partial far above that isn't a harmonic, and
        // a second tooth a few cents sharp. It rings on after the key is let go.
        { "Music Box", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 1.0f, 12.0f, { 1.0f, 0.0f, 0.06f });
            setPartials (synth, osc2, 0.3f, 12.05f, { 1.0f });

            // 6.27 times the tooth's note, as a bar fixed at one end has: the 3rd harmonic of an octave above it,
            // tuned up.
            setPartials (synth, osc3, 0.0f, 24.0f, { 0.0f, 0.0f, 1.0f });
            synth.setHarmonicTune (osc3, 3, 0.135f);
            setEnvelope (synth, 1, { 0.0f, 0.3f, 0.0f, 0.3f });
            assign (synth, oscillatorLevelTarget (osc3), env2, 0.3f);
            assignVelocity (synth, oscillatorLevelTarget (osc3), 0.2f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.0f, 1.8f, 0.0f, 1.2f });
            assign (synth, volumeTarget, env1, 0.72f);
        } },

        // A string plucked a fifth of the way along, which leaves out every 5th harmonic. The low harmonics ring on
        // while the high ones, on another oscillator, are gone in under a second, and softer notes are plucked less
        // brightly. The low strings, a pitch set, ring longer.
        { "Nylon Guitar", [] (SynthProcessor& synth)
        {
            const auto make = [&synth] (float seconds, float highSeconds)
            {
                // Each harmonic as plucking a fifth of the way along leaves it, falling away by its number squared.
                const auto pluck = [] (int harmonic) { return (float) (std::abs (std::sin (pi * harmonic * 0.2)) / std::pow (harmonic, 1.7) / std::sin (pi * 0.2)); };

                setPartials (synth, osc1, 1.0f, 0.0f, { pluck (1), pluck (2), pluck (3) });
                setPartials (synth, osc2, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.75f, 0.6f, 0.45f, 0.35f, 0.0f, 0.2f, 0.16f, 0.13f, 0.1f });
                setEnvelope (synth, 1, { 0.0f, highSeconds, 0.0f, 0.2f });
                assign (synth, oscillatorLevelTarget (osc2), env2, 0.22f);
                assignVelocity (synth, oscillatorLevelTarget (osc2), 0.18f);

                // The body: a resonance near the bottom of its range.
                setFilter (synth, 1, FilterType::highPass, 95.0f, 0.35f);
                setFilter (synth, 0, FilterType::lowPass, 3200.0f);
                assignVelocity (synth, filterCutoffTarget (0), 0.3f);

                setRing (synth, lfo1, seconds, 0.8f, [seconds] (double t) { return 0.5 * std::exp (-t / (0.08 * seconds)) + 0.5 * std::exp (-t / (0.3 * seconds)); });
            };

            make (4.0f, 0.8f);

            addPitchSet (synth, 0, 52);
            make (6.5f, 1.3f);
            synth.setActivePitchSet (0);
        } },

        // A pipe organ's ranks, each an oscillator: the 16', 8', 4', 2 2/3' and 2' pipes, an octave below the note up
        // to two octaves above it, and a rank a little sharp that beats against the rest. Each pipe speaks with a
        // little chiff, and every note is as loud.
        { "Pipe Organ", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.8f, 0.0f, { 1.0f, 0.5f, 0.33f, 0.2f, 0.12f, 0.08f });
            setPartials (synth, osc2, 0.5f, -12.0f, { 1.0f, 0.3f, 0.15f });
            setPartials (synth, osc3, 0.4f, 12.0f, { 1.0f, 0.4f, 0.2f });
            setPartials (synth, osc4, 0.22f, 19.0f, { 1.0f, 0.3f });
            setPartials (synth, osc5, 0.1f, 24.0f, { 1.0f, 0.3f });
            setPartials (synth, osc6, 0.2f, 0.07f, { 1.0f, 0.4f, 0.2f });

            // The chiff, as the highest rank speaks.
            setEnvelope (synth, 1, { 0.0f, 0.07f, 0.0f, 0.05f });
            assign (synth, oscillatorLevelTarget (osc5), env2, 0.35f);

            synth.setVelocityAssignment (volumeTarget, {});
            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.05f, 0.1f, 1.0f, 0.2f });
            assign (synth, volumeTarget, env1, 0.55f);
        } },

        // Three sawtooths a fifth of a semitone apart, grinding against each other over a sub, through a low pass
        // and a notch that an LFO slowly sweeps. Legato notes slide, and the mod wheel opens the filter.
        { "Reese Bass", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.6f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.6f);
            setOscillator (synth, osc3, fallingSawtooth(), 16, 0.6f);
            setTune (synth, osc2, 0.18f);
            setTune (synth, osc3, -0.18f);
            setOscillator (synth, osc4, sine, 1, 0.6f);
            setTune (synth, osc4, -12.0f);
            setGlide (synth, { osc1, osc2, osc3, osc4 }, Glide::legato, 0.1f);
            setParameter (synth, SynthProcessor::volumeId, 0.55f);

            setFilter (synth, 0, FilterType::lowPass, 700.0f, 0.15f);
            assignModWheel (synth, filterCutoffTarget (0), 0.4f);

            setFilter (synth, 3, FilterType::notch, 800.0f);
            setParameter (synth, SynthProcessor::filterIds (3).q, 3.0f);
            setParameter (synth, SynthProcessor::filterIds (3).depth, 30.0f);
            setLfo (synth, lfo1, 1.0f / 0.13f, loops, triangle);
            assign (synth, filterCutoffTarget (3), lfo1, 0.4f);
        } },

        // A resonant low pass whose cutoff jumps to a new random place four times a beat, as a sample and hold does:
        // an LFO playing random steps, synced to the tempo. A slower random LFO moves the resonance.
        { "Sample and Hold", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, square, 16, 0.8f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.4f);
            setTune (synth, osc2, -12.0f);
            setParameter (synth, SynthProcessor::volumeId, 0.55f);

            setFilter (synth, 0, FilterType::lowPass, 900.0f, 0.55f);
            setLfo (synth, lfo1, 2.0f, loops, sine);
            setRandom (synth, lfo1, true, 8);
            setSync (synth, lfo1, 0.5);
            assign (synth, filterCutoffTarget (0), lfo1, 0.55f);

            setLfo (synth, lfo2, 5.0f, loops, sine);
            setRandom (synth, lfo2, false);
            assign (synth, filterResonanceTarget (0), lfo2, 0.3f);
        } },

        // Two oscillators whose harmonics are randomized again and again, each time its own slow LFO crosses its
        // middle, so the sound is never the same for long. Another LFO sweeps how many harmonics the first plays.
        { "Shifting Sands", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.8f, 0.0f, { 1.0f, 0.5f, 0.3f, 0.4f, 0.2f, 0.25f, 0.1f, 0.15f, 0.1f });
            setPartials (synth, osc2, 0.4f, 12.0f, { 1.0f, 0.3f, 0.4f, 0.2f, 0.1f });

            setLfo (synth, lfo1, 4.0f, loops, sine);
            trigger (synth, randomizeTarget (osc1), lfo1, 0.5f);
            setLfo (synth, lfo2, 5.6f, loops, triangle);
            trigger (synth, randomizeTarget (osc2), lfo2, 0.5f);

            setLfo (synth, lfo3, 7.0f, loops, sine);
            assign (synth, oscillatorHarmonicsTarget (osc1), lfo3, 0.5f);

            setFilter (synth, 0, FilterType::lowPass, 2200.0f, 0.1f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.8f, 1.0f, 0.85f, 2.0f });
            assign (synth, volumeTarget, env1, 0.55f);
        } },

        // Two squares a tenth of a semitone apart over a sub, between a high pass and a resonant low pass, sliding
        // between notes. A vibrato starts with each note, and the mod wheel opens the filter.
        { "Square Lead", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, square, 16, 0.8f);
            setOscillator (synth, osc2, square, 16, 0.4f);
            setTune (synth, osc2, 0.1f);
            setOscillator (synth, osc3, sine, 1, 0.4f);
            setTune (synth, osc3, -12.0f);
            setGlide (synth, { osc1, osc2, osc3 }, Glide::always, 0.05f);
            setParameter (synth, SynthProcessor::volumeId, 0.55f);

            setFilter (synth, 1, FilterType::highPass, 150.0f);
            setFilter (synth, 0, FilterType::lowPass, 2400.0f, 0.35f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f);

            setLfo (synth, lfo1, 1.0f / 5.5f, fromKey, sine);

            for (const auto oscillator : { osc1, osc2 })
            {
                setTuneReach (synth, oscillator, 1.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo1, 0.08f);
            }
        } },

        // An LFO drawn as eight steps moves a resonant low pass filter's cutoff, like a step sequencer, in time with
        // the tempo: a step every half beat.
        { "Step Filter", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 1.0f);
            setOscillator (synth, osc2, square, 9, 0.4f);
            setTune (synth, osc2, -12.0f);
            setParameter (synth, SynthProcessor::volumeId, 0.6f);

            setFilter (synth, 0, FilterType::lowPass, 700.0f, 0.55f);
            setLfo (synth, lfo2, 2.0f, loops, sine);
            setSteps (synth, lfo2, { -0.8, 0.6, -0.2, 1.0, -0.6, 0.3, -1.0, 0.8 });
            setSync (synth, lfo2, 0.25);
            assign (synth, filterCutoffTarget (0), lfo2, 0.6f);
        } },

        // A held key plays a tune: an LFO drawn in steps moves the oscillators' pitch through the notes of a minor
        // chord, sixteen to the bar, starting with each key, while a second, four times as fast, plucks each one.
        { "Stepped Arp", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, square, 9, 0.8f);
            setOscillator (synth, osc2, fallingSawtooth(), 12, 0.4f);
            setParameter (synth, SynthProcessor::volumeId, 0.62f);

            // Each step is so many twelfths of an octave above the key, 0, 3, 7, 12, 7, 3, 0 and 5 below, to the
            // thousandth a sound description keeps: at full reach, a range of a half swings the tune an octave
            // either way.
            setLfo (synth, lfo1, 1.0f, fromKey, sine);
            setSteps (synth, lfo1, { 0.0, 0.25, 0.583, 1.0, 0.583, 0.25, 0.0, -0.417 });
            setSync (synth, lfo1, 0.5);

            for (const auto oscillator : { osc1, osc2 })
                assign (synth, oscillatorTuneTarget (oscillator), lfo1, 0.5f);

            // The pluck: the volume falls through each step, as a sawtooth, inverted, falls.
            setLfo (synth, lfo2, 0.125f, fromKey, waveform::Preset::sawtooth);
            setSync (synth, lfo2, 4.0);
            assign (synth, volumeTarget, lfo2, 0.3f, true);

            setFilter (synth, 0, FilterType::lowPass, 2600.0f, 0.25f);
            assign (synth, filterCutoffTarget (0), lfo2, 0.2f, true);
        } },

        // A string section: sawtooths a few cents apart, two of them drifting slowly in and out of tune, swelling in
        // and fading out. The low keys, a pitch set, are darker, with an octave below; the high keys, another, are
        // brighter and speak sooner.
        { "String Ensemble", [] (SynthProcessor& synth)
        {
            const auto make = [&synth] (float cutoff, float attack, float octaveTune)
            {
                setOscillator (synth, osc1, fallingSawtooth(), 16, 0.6f);
                setOscillator (synth, osc2, fallingSawtooth(), 16, 0.5f);
                setOscillator (synth, osc3, fallingSawtooth(), 16, 0.5f);
                setOscillator (synth, osc4, fallingSawtooth(), 9, 0.25f);
                setTune (synth, osc2, 0.09f);
                setTune (synth, osc3, -0.08f);
                setTune (synth, osc4, octaveTune);

                setLfo (synth, lfo1, 1.0f / 0.31f, loops, sine);
                setLfo (synth, lfo2, 1.0f / 0.43f, loops, sine);
                setTuneReach (synth, osc2, 1.0);
                setTuneReach (synth, osc3, 1.0);
                assign (synth, oscillatorTuneTarget (osc2), lfo1, 0.06f);
                assign (synth, oscillatorTuneTarget (osc3), lfo2, 0.06f);

                setFilter (synth, 0, FilterType::lowPass, cutoff, 0.05f);
                assignVelocity (synth, filterCutoffTarget (0), 0.15f);

                setParameter (synth, SynthProcessor::volumeId, 0.0f);
                setEnvelope (synth, 0, { attack, 0.6f, 0.9f, 0.7f });
                assign (synth, volumeTarget, env1, 0.68f);
            };

            make (2600.0f, 0.45f, 12.05f);

            addPitchSet (synth, 0, 47);
            make (1500.0f, 0.55f, -11.95f);

            addPitchSet (synth, 72, 127);
            make (4200.0f, 0.3f, 12.05f);
            synth.setActivePitchSet (0);
        } },

        // A voice singing "ah": two sawtooths through a resonant high pass and a resonant low pass, whose peaks stand
        // for a vowel's two formants. The mod wheel moves them apart, towards "ee", and an LFO gives it a vibrato.
        { "Vox Pad", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.7f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.5f);
            setTune (synth, osc2, 0.07f);

            setFilter (synth, 1, FilterType::highPass, 620.0f, 0.5f);
            setFilter (synth, 0, FilterType::lowPass, 1250.0f, 0.55f);
            assignModWheel (synth, filterCutoffTarget (0), 0.12f);
            assignModWheel (synth, filterCutoffTarget (1), 0.12f, true);

            setLfo (synth, lfo1, 1.0f / 5.0f, loops, sine);

            for (const auto oscillator : { osc1, osc2 })
            {
                setTuneReach (synth, oscillator, 1.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo1, 0.06f);
            }

            setLfo (synth, lfo2, 7.0f, loops, sine);
            assign (synth, filterCutoffTarget (0), lfo2, 0.05f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.35f, 0.5f, 0.9f, 0.6f });
            assign (synth, volumeTarget, env1, 0.6f);
        } },

        // A triangle, a sawtooth a little sharp, a sine and a triangle an octave up, swelling in and fading slowly,
        // through a low pass filter that one LFO slowly opens and closes, while another makes the sawtooth shimmer.
        { "Warm Pad", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, triangle, 5, 0.8f);
            setOscillator (synth, osc2, fallingSawtooth(), 9, 0.35f);
            setTune (synth, osc2, 0.06f);
            setOscillator (synth, osc3, sine, 1, 0.5f);
            setOscillator (synth, osc4, triangle, 3, 0.2f);
            setTune (synth, osc4, 11.96f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 2, { 1.5f, 1.0f, 0.85f, 2.5f });
            assign (synth, volumeTarget, env3, 0.6f);

            setFilter (synth, 0, FilterType::lowPass, 900.0f, 0.15f);
            setLfo (synth, lfo1, 6.0f, loops, sine);
            assign (synth, filterCutoffTarget (0), lfo1, 0.3f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f);

            setLfo (synth, lfo2, 3.0f, loops, triangle);
            assign (synth, oscillatorLevelTarget (osc2), lfo2, 0.5f);
        } },

        // A square, a sawtooth and a sub through a resonant low pass that an LFO wobbles twice a beat, in time with
        // the tempo. Legato notes slide, and the mod wheel sharpens the resonance.
        { "Wobble Bass", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, square, 16, 0.9f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.4f);
            setTune (synth, osc2, 0.1f);
            setOscillator (synth, osc3, sine, 1, 0.6f);
            setTune (synth, osc3, -12.0f);
            setGlide (synth, { osc1, osc2, osc3 }, Glide::legato, 0.08f);
            setParameter (synth, SynthProcessor::volumeId, 0.5f);

            setFilter (synth, 0, FilterType::lowPass, 400.0f, 0.5f);
            setLfo (synth, lfo1, 0.25f, loops, sine);
            setSync (synth, lfo1, 2.0);
            assign (synth, filterCutoffTarget (0), lfo1, 0.5f);
            assignModWheel (synth, filterResonanceTarget (0), 0.3f);
        } },
    };
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "Presets");
    auto failed = false;

    // Only these are the factory presets: any others there, from recipes since taken away, go.
    for (const auto& old : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + SynthProcessor::presetExtension))
        old.deleteFile();

    for (const auto& recipe : recipes)
    {
        SynthProcessor synth;
        recipe.make (synth);

        const auto file = folder.getChildFile (recipe.name);
        const auto saved = synth.savePreset (file);
        failed = failed || ! saved;

        std::cout << (saved ? "Saved " : "Couldn't save ") << file.withFileExtension (SynthProcessor::presetExtension).getFullPathName()
                  << std::endl;
    }

    return failed ? 1 : 0;
}
