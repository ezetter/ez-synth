// Makes twenty strange pads that go on changing for as long as a key is held, some over cycles as long as 20 seconds,
// and saves them among your own presets, in ~/Documents/EZ Synth/Presets unless another folder is named, in the Pads
// category with the label Evolving. They aren't factory presets: they're yours to change, save over or delete.
//
//     ./build/EZSynthMakeEvolvingPads
//
// Each recipe starts from Init, the sound EZ Synth starts with, and changes what it needs to. Some change the same
// way every time, some are moved by random LFOs, and some randomize or switch parts of themselves as they play, so no
// two of those are heard alike. A preset there already under one of these names is only saved over if it has the
// Evolving label too.

#include "RecipeTools.h"

#include <iostream>

namespace
{
    using namespace modulation;
    using namespace recipe;

    constexpr auto category = "Pads";
    constexpr auto label = "Evolving";
    constexpr int env8 = envelopeSource (7);

    /** Fades the whole sound in and out with the key, on the last envelope, up to a volume. */
    void swell (SynthProcessor& synth, float attack, float release, float volume)
    {
        setParameter (synth, SynthProcessor::volumeId, 0.0f);
        setEnvelope (synth, 7, { attack, 1.0f, 1.0f, release });
        assign (synth, volumeTarget, env8, volume);
    }

    /** An LFO that loops its wave all the time. */
    void slow (SynthProcessor& synth, int lfo, float seconds, waveform::Preset preset)
    {
        setLfo (synth, lfo, seconds, loops, preset);
    }

    /** An LFO that wanders: a new smooth random wave every cycle. */
    void drift (SynthProcessor& synth, int lfo, float seconds)
    {
        setLfo (synth, lfo, seconds, loops, sine);
        setRandom (synth, lfo, false);
    }

    /** An LFO that jumps: new random steps every cycle. */
    void jumps (SynthProcessor& synth, int lfo, float seconds, int numSteps)
    {
        setLfo (synth, lfo, seconds, loops, sine);
        setRandom (synth, lfo, true, numSteps);
    }

    /** From 0 to 1, smoothly, as x goes from 0 to 1. */
    double ease (double x)
    {
        x = juce::jlimit (0.0, 1.0, x);
        return x * x * (3.0 - 2.0 * x);
    }

    /** Where a cutoff sits along its travel, from 0 to 1. */
    double place (double hertz)   { return (double) FilterStage::cutoffToPosition ((float) hertz); }

    /** Has an LFO take a filter's cutoff through a list of frequencies, one after another, holding each for a while
        and gliding to the next, and back to the first as it loops.
    */
    void tour (SynthProcessor& synth, int lfo, int filter, const std::vector<double>& hertz)
    {
        auto lowest = 1.0, highest = 0.0;

        for (const auto frequency : hertz)
        {
            lowest = std::min (lowest, place (frequency));
            highest = std::max (highest, place (frequency));
        }

        const auto middle = (lowest + highest) / 2.0, range = highest - lowest;

        synth.setLfoShape (lfo, draw ([&] (double t)
        {
            const auto along = t * (double) hertz.size();
            const auto from = (size_t) along % hertz.size(), to = (from + 1) % hertz.size();
            const auto glide = ease ((along - std::floor (along) - 0.5) / 0.5);
            const auto position = place (hertz[from]) + (place (hertz[to]) - place (hertz[from])) * glide;
            return 2.0 * (position - middle) / range;
        }));

        setParameter (synth, SynthProcessor::filterIds (filter).cutoff, FilterStage::positionToCutoff ((float) middle));
        assign (synth, filterCutoffTarget (filter), lfo, (float) range);
    }

    const std::vector<float> organ { 1.0f, 0.6f, 0.5f, 0.4f, 0.35f, 0.3f, 0.25f, 0.22f, 0.2f, 0.18f, 0.16f, 0.14f, 0.12f, 0.1f, 0.09f, 0.08f };

    //==============================================================================
    const std::vector<Recipe> recipes {
        // Falling in: an LFO that runs ever faster for 20 seconds and then starts again speeds four others up with
        // it, from a slow sway to a blur, which move the levels, the pans and the low pass, while the pitch falls an
        // octave, the unison spreads out of tune, and it all snaps back. Nothing's random.
        { "Event Horizon", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 14, 0.5f);
            setOscillator (synth, osc2, square, 9, 0.5f);
            setTune (synth, osc2, -12.0f);
            setPartials (synth, osc3, 0.5f, 7.02f, { 1.0f, 0.4f, 0.3f, 0.1f });
            setPartials (synth, osc4, 0.5f, -24.0f, { 1.0f });
            setUnison (synth, osc1, 3, 10.0f, 0.7f);

            // The fall: slow at first, then headlong.
            setLfo (synth, lfo8, 20.0f, loops, draw ([] (double t) { return 2.0 * t * t * t - 1.0; }));

            for (const auto oscillator : { osc1, osc2, osc3 })
                assign (synth, oscillatorTuneTarget (oscillator), lfo8, 0.25f, true);

            setParameter (synth, SynthProcessor::unisonDetuneId (osc1), 50.0f);
            assign (synth, unisonDetuneTarget (osc1), lfo8, 0.8f);

            slow (synth, lfo1, 2.0f, sine);
            slow (synth, lfo2, 3.0f, sine);
            slow (synth, lfo3, 2.5f, triangle);
            slow (synth, lfo4, 1.7f, sine);

            for (const auto lfo : { lfo1, lfo2, lfo3, lfo4 })
                assign (synth, lfoRateTarget (lfo), lfo8, 0.6f);

            setParameter (synth, SynthProcessor::levelId (osc1), 0.5f);
            assign (synth, oscillatorLevelTarget (osc1), lfo1, 0.9f);
            assign (synth, oscillatorPanTarget (osc2), lfo2, 0.9f);
            assign (synth, oscillatorPanTarget (osc3), lfo2, 0.9f, true);
            assign (synth, oscillatorLevelTarget (osc3), lfo4, 0.9f);

            setFilter (synth, 0, FilterType::lowPass, 1400.0f, 0.45f);
            assign (synth, filterCutoffTarget (0), lfo3, 0.35f);
            setSends (synth, osc4, {});
            assignModWheel (synth, filterResonanceTarget (0), 0.4f);

            swell (synth, 1.5f, 3.0f, 0.6f);
        } },

        // Two oscillators with the same odd harmonics, one's phases wandering on two random LFOs, so each harmonic
        // in turn is doubled or hollowed out as they add and cancel. Over them a stack of eight voices breathes in
        // and out every nine seconds, its detune widening as it fills.
        { "Glass Lungs", [] (SynthProcessor& synth)
        {
            const std::vector<float> glass { 1.0f, 0.0f, 0.6f, 0.0f, 0.45f, 0.0f, 0.35f, 0.0f, 0.3f, 0.0f, 0.25f, 0.0f, 0.2f, 0.0f, 0.15f };
            setPartials (synth, osc1, 0.6f, 0.0f, glass);
            setPartials (synth, osc2, 0.6f, 0.0f, glass);
            setPan (synth, osc1, 0.35f);
            setPan (synth, osc2, 0.65f);

            drift (synth, lfo2, 11.0f);
            drift (synth, lfo3, 14.0f);

            for (int harmonic = 1; harmonic <= 15; harmonic += 2)
                assign (synth, harmonicPhaseTarget (osc2, harmonic), harmonic % 4 == 1 ? lfo2 : lfo3, 1.0f, false, (float) harmonic / 16.0f);

            // The breath: in over four seconds, out over five.
            setOscillator (synth, osc3, triangle, 7, 0.3f);
            setTune (synth, osc3, 12.0f);
            setUnison (synth, osc3, 8, 40.0f, 1.0f);
            setLfo (synth, lfo1, 9.0f, loops, draw ([] (double t) { return t < 0.45 ? 2.0 * ease (t / 0.45) - 1.0 : 1.0 - 2.0 * ease ((t - 0.45) / 0.55); }));
            assign (synth, oscillatorLevelTarget (osc3), lfo1, 0.6f);
            assign (synth, unisonDetuneTarget (osc3), lfo1, 0.7f);

            setFilter (synth, 0, FilterType::lowPass, 3000.0f, 0.2f);
            assign (synth, filterCutoffTarget (0), lfo1, 0.15f);
            assignModWheel (synth, filterCutoffTarget (0), 0.25f);

            swell (synth, 2.0f, 4.0f, 0.66f);
        } },

        // Six sines at pitches no scale has, the 7th, 11th and 13th harmonics among them. Each one's level, pan and
        // tune jump on random steps, a different LFO for each, and a seventh LFO, over 20 seconds, takes all six from
        // a slow flutter to a frantic one and back.
        { "Moth Swarm", [] (SynthProcessor& synth)
        {
            constexpr std::array tunes { 0.0f, 2.04f, 5.51f, 8.41f, 9.69f, 14.04f };
            constexpr std::array periods { 0.9f, 1.3f, 0.7f, 1.7f, 1.1f, 2.3f };

            for (int i = 0; i < 6; ++i)
            {
                setPartials (synth, i, 0.45f, tunes[(size_t) i], { 1.0f, 0.0f, 0.2f });
                jumps (synth, i, periods[(size_t) i], 8);
                assign (synth, oscillatorLevelTarget (i), i, 0.9f);
                assign (synth, oscillatorPanTarget (i), (i + 1) % 6, 0.9f);
                setTuneReach (synth, i, 1.0);
                assign (synth, oscillatorTuneTarget (i), (i + 3) % 6, 0.3f);
                assign (synth, lfoRateTarget (i), lfo7, 0.5f);
            }

            slow (synth, lfo7, 20.0f, sine);

            setFilter (synth, 1, FilterType::highPass, 120.0f);
            setFilter (synth, 0, FilterType::lowPass, 5000.0f, 0.15f);
            drift (synth, lfo8, 6.0f);
            assign (synth, filterCutoffTarget (0), lfo8, 0.3f);

            swell (synth, 1.0f, 2.5f, 0.66f);
        } },

        // A bell that's struck again and again under water: its hum every 7 seconds, its minor third every 11 and its
        // upper partials every 13, each on an LFO that starts with the key, so they never fall together twice. The
        // whole bell sinks and rises a quarter tone over 20 seconds, its partials wander out of tune, and a wandering
        // low pass and a sweeping notch stand for the water.
        { "Drowned Bells", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.5f, -12.0f, { 0.5f, 0.9f, 0.2f });
            setPartials (synth, osc2, 0.3f, 3.16f, { 1.0f });
            setPartials (synth, osc3, 0.4f, -12.0f, { 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.35f, 0.0f, 0.3f, 0.0f, 0.0f, 0.15f, 0.0f, 0.1f });
            synth.setHarmonicTune (osc3, 5, 0.33f);
            synth.setHarmonicTune (osc3, 11, -0.33f);
            synth.setHarmonicTune (osc3, 13, 0.33f);
            setPan (synth, osc2, 0.25f);
            setPan (synth, osc3, 0.75f);

            const auto strike = draw ([] (double t) { return 2.0 * std::exp (-6.0 * t) * std::min (1.0, t * 400.0) - 1.0; });
            setLfo (synth, lfo1, 7.0f, fromKey, strike);
            setLfo (synth, lfo2, 11.0f, fromKey, strike);
            setLfo (synth, lfo3, 13.0f, fromKey, strike);
            assign (synth, oscillatorLevelTarget (osc1), lfo1, 1.0f);
            assign (synth, oscillatorLevelTarget (osc2), lfo2, 0.6f);
            assign (synth, oscillatorLevelTarget (osc3), lfo3, 0.8f);

            slow (synth, lfo4, 20.0f, triangle);

            for (const auto oscillator : { osc1, osc2, osc3 })
            {
                setTuneReach (synth, oscillator, 2.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo4, 0.25f);
            }

            drift (synth, lfo5, 9.0f);

            for (const auto harmonic : { 5, 6, 8, 11, 13 })
                assign (synth, harmonicTuneTarget (osc3, harmonic), lfo5, 0.2f, harmonic % 2 == 0, (float) harmonic / 13.0f);

            setFilter (synth, 0, FilterType::lowPass, 1800.0f, 0.4f);
            drift (synth, lfo6, 5.0f);
            assign (synth, filterCutoffTarget (0), lfo6, 0.25f);

            setFilter (synth, 3, FilterType::notch, 700.0f);
            setParameter (synth, SynthProcessor::filterIds (3).q, 2.0f);
            slow (synth, lfo7, 17.0f, sine);
            assign (synth, filterCutoffTarget (3), lfo7, 0.4f);

            swell (synth, 0.01f, 6.0f, 0.75f);
        } },

        // Three bands of noise, each through a resonant low pass of its own that makes it whistle, a sine through
        // none of them, and a fifth far above that comes and goes. Random LFOs move each whistle, one in steps, and
        // over 20 seconds the resonances go from hiss to whistle and back. The low keys and the high keys are pitch
        // sets, with the whistles an octave lower and higher.
        { "Static Garden", [] (SynthProcessor& synth)
        {
            const auto make = [&synth] (float octaves)
            {
                const auto scale = std::pow (2.0f, octaves);
                constexpr std::array cutoffs { 420.0f, 1300.0f, 3600.0f };
                constexpr std::array pans { 0.2f, 0.8f, 0.5f };

                for (int band = 0; band < 3; ++band)
                {
                    setNoise (synth, band, 0.5f);
                    setSends (synth, band, { band, 3 });
                    setPan (synth, band, pans[(size_t) band]);
                    setFilter (synth, band, FilterType::lowPass, cutoffs[(size_t) band] * scale, 0.75f);
                }

                setFilter (synth, 3, FilterType::highPass, 200.0f * scale);

                drift (synth, lfo1, 8.0f);
                assign (synth, filterCutoffTarget (0), lfo1, 0.12f);
                drift (synth, lfo2, 13.0f);
                assign (synth, filterCutoffTarget (1), lfo2, 0.15f);
                jumps (synth, lfo3, 6.0f, 4);
                assign (synth, filterCutoffTarget (2), lfo3, 0.12f);

                slow (synth, lfo4, 20.0f, sine);

                for (int band = 0; band < 3; ++band)
                    assign (synth, filterResonanceTarget (band), lfo4, 0.5f, false, (float) band / 3.0f);

                drift (synth, lfo5, 7.0f);
                assign (synth, oscillatorPanTarget (osc3), lfo5, 0.9f);

                setPartials (synth, osc4, 0.3f, 0.0f, { 1.0f, 0.0f, 0.1f });
                setSends (synth, osc4, {});
                setPartials (synth, osc5, 0.1f, 19.02f, { 1.0f });
                setSends (synth, osc5, {});
                drift (synth, lfo6, 10.0f);
                assign (synth, oscillatorLevelTarget (osc5), lfo6, 0.2f);

                swell (synth, 2.0f, 4.0f, 0.72f);
            };

            make (0.0f);
            addPitchSet (synth, 0, 52);
            make (-1.0f);
            addPitchSet (synth, 72, 127);
            make (1.0f);
            synth.setActivePitchSet (0);
        } },

        // A triangle LFO, 16 seconds there and back, switches a sawtooth's harmonics on one at a time as it rises,
        // from the 2nd to the 16th, and off again as it falls, each at its own threshold, like an overtone singer
        // climbing the series. A second, 11 seconds, does the same to an oscillator a fifth above, out of step.
        // Nothing's random, but the two only line up every three minutes.
        { "Overtone Clockwork", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.7f, 0.0f, { 1.0f, 0.8f, 0.7f, 0.65f, 0.6f, 0.55f, 0.5f, 0.5f, 0.45f, 0.45f, 0.4f, 0.4f, 0.35f, 0.35f, 0.3f, 0.3f });
            setPartials (synth, osc2, 0.4f, 7.02f, { 1.0f, 0.7f, 0.6f, 0.5f, 0.45f, 0.4f, 0.35f, 0.3f });
            setPartials (synth, osc3, 0.5f, -12.0f, { 1.0f, 0.3f });
            setPan (synth, osc1, 0.4f);
            setPan (synth, osc2, 0.7f);

            slow (synth, lfo1, 16.0f, triangle);
            slow (synth, lfo2, 11.0f, triangle);

            for (int harmonic = 2; harmonic <= 16; ++harmonic)
            {
                synth.setHarmonicSwitch (osc1, harmonic, waveform::HarmonicSwitch::off);
                trigger (synth, harmonicSwitchTarget (osc1, harmonic), lfo1, (float) harmonic / 17.0f);
            }

            for (int harmonic = 2; harmonic <= 8; ++harmonic)
            {
                synth.setHarmonicSwitch (osc2, harmonic, waveform::HarmonicSwitch::off);
                trigger (synth, harmonicSwitchTarget (osc2, harmonic), lfo2, (float) harmonic / 9.0f);
            }

            slow (synth, lfo3, 20.0f, sine);
            assign (synth, oscillatorPanTarget (osc1), lfo3, 0.5f);

            setFilter (synth, 0, FilterType::lowPass, 6000.0f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f, true);

            swell (synth, 0.8f, 2.5f, 0.6f);
        } },

        // One voice of a sawtooth grows to eight over 17 seconds, their detune widening to a whole tone across and
        // their spread to the full width, while a low pass opens and a rush of noise rises; then the whole mass
        // falls in on itself in three seconds, and it starts again.
        { "Slow Avalanche", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.8f);
            setOscillator (synth, osc2, square, 9, 0.4f);
            setTune (synth, osc2, -12.0f);
            setPartials (synth, osc3, 0.5f, -24.0f, { 1.0f });
            setSends (synth, osc3, {});

            setLfo (synth, lfo1, 20.0f, loops, draw ([] (double t) { return t < 0.85 ? 2.0 * std::pow (t / 0.85, 1.5) - 1.0 : 1.0 - 2.0 * ease ((t - 0.85) / 0.15); }));

            for (const auto oscillator : { osc1, osc2 })
            {
                setUnison (synth, oscillator, 4, 50.0f, 0.5f);
                assign (synth, unisonVoicesTarget (oscillator), lfo1, 1.0f);
                assign (synth, unisonDetuneTarget (oscillator), lfo1, 1.0f);
                assign (synth, unisonSpreadTarget (oscillator), lfo1, 1.0f);
            }

            setNoise (synth, osc4, 0.12f);
            assign (synth, oscillatorLevelTarget (osc4), lfo1, 0.24f);

            setFilter (synth, 0, FilterType::lowPass, 1200.0f, 0.3f);
            assign (synth, filterCutoffTarget (0), lfo1, 0.5f);
            assignVelocity (synth, filterCutoffTarget (0), 0.15f);

            swell (synth, 1.5f, 3.0f, 0.62f);
        } },

        // A mouth that isn't one: sawtooths through a resonant high pass and low pass, a vowel's two formants, which
        // two LFOs take through "ah", "eh", "ee", "oh" and "oo" and back round, 20 seconds a round, holding each and
        // sliding to the next. A wandering LFO bends the pitch, and the mod wheel sharpens the vowels.
        { "Vowel Drift", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 16, 0.7f);
            setUnison (synth, osc1, 3, 9.0f, 0.6f);
            setOscillator (synth, osc2, fallingSawtooth(), 16, 0.5f);
            setTune (synth, osc2, -12.0f);
            setNoise (synth, osc3, 0.12f);

            setFilter (synth, 1, FilterType::highPass, 450.0f, 0.65f);
            setFilter (synth, 0, FilterType::lowPass, 1300.0f, 0.65f);
            setLfo (synth, lfo1, 20.0f, loops, sine);
            setLfo (synth, lfo2, 20.0f, loops, sine);
            tour (synth, lfo1, 1, { 700.0, 450.0, 300.0, 450.0, 320.0 });
            tour (synth, lfo2, 0, { 1150.0, 1900.0, 2300.0, 850.0, 700.0 });
            assignModWheel (synth, filterResonanceTarget (0), 0.3f);
            assignModWheel (synth, filterResonanceTarget (1), 0.3f);

            drift (synth, lfo3, 3.0f);

            for (const auto oscillator : { osc1, osc2 })
            {
                setTuneReach (synth, oscillator, 1.0);
                assign (synth, oscillatorTuneTarget (oscillator), lfo3, 0.15f);
            }

            swell (synth, 0.8f, 2.0f, 0.66f);
        } },

        // A wave travels up an oscillator's sixteen harmonics every 12 seconds, each one's amplitude on the same
        // LFO a sixteenth of a cycle after the last, while another travels down the harmonics of an oscillator an
        // octave up every 17. The two cross from side to side over 20. Nothing's random.
        { "Spectral Tide", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.7f, 0.0f, std::vector<float> (16, 0.35f));
            setPartials (synth, osc2, 0.45f, 12.02f, std::vector<float> (12, 0.35f));
            setPartials (synth, osc3, 0.5f, -12.0f, { 1.0f, 0.2f });

            slow (synth, lfo1, 12.0f, sine);
            slow (synth, lfo2, 17.0f, sine);

            for (int harmonic = 1; harmonic <= 16; ++harmonic)
                assign (synth, harmonicTarget (osc1, harmonic), lfo1, 1.0f, false, (float) (harmonic - 1) / 16.0f);

            for (int harmonic = 1; harmonic <= 12; ++harmonic)
                assign (synth, harmonicTarget (osc2, harmonic), lfo2, 1.0f, false, (float) (12 - harmonic) / 12.0f);

            slow (synth, lfo3, 20.0f, sine);
            assign (synth, oscillatorPanTarget (osc1), lfo3, 0.8f);
            assign (synth, oscillatorPanTarget (osc2), lfo3, 0.8f, true);

            swell (synth, 1.5f, 3.5f, 0.66f);
        } },

        // A hymn on a radio nobody's tuned: a thin square whose pitch jumps to anywhere within an octave either way
        // twice a second, through a mid pass that jumps too, over an organ chord that drops in and out and bursts of
        // static. All of it's on LFOs playing random steps, so it never plays the same tune.
        { "Broken Radio Hymn", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, square, 7, 0.7f);
            setSends (synth, osc1, { 2 });
            jumps (synth, lfo1, 4.0f, 8);
            assign (synth, oscillatorTuneTarget (osc1), lfo1, 0.5f);
            setGlide (synth, { osc1 }, Glide::always, 0.3f);

            setPartials (synth, osc2, 0.3f, 0.0f, organ);
            setPartials (synth, osc3, 0.3f, 7.02f, { 1.0f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f });
            setUnison (synth, osc2, 2, 6.0f, 0.8f);

            for (const auto oscillator : { osc2, osc3 })
                setSends (synth, oscillator, { 0 });

            jumps (synth, lfo4, 9.0f, 10);
            assign (synth, oscillatorLevelTarget (osc2), lfo4, 0.6f);
            assign (synth, oscillatorLevelTarget (osc3), lfo4, 0.6f, true);
            setFilter (synth, 0, FilterType::lowPass, 1600.0f, 0.2f);

            setNoise (synth, osc4, 0.0f);
            setSends (synth, osc4, { 2 });
            jumps (synth, lfo3, 3.0f, 6);
            assign (synth, oscillatorLevelTarget (osc4), lfo3, 0.7f);

            setFilter (synth, 2, FilterType::midPass, 1500.0f);
            setParameter (synth, SynthProcessor::filterIds (2).width, 1.2f);
            jumps (synth, lfo2, 6.0f, 4);
            assign (synth, filterCutoffTarget (2), lfo2, 0.3f);

            drift (synth, lfo5, 5.0f);
            assign (synth, oscillatorPanTarget (osc1), lfo5, 0.9f);
            assign (synth, oscillatorPanTarget (osc4), lfo5, 0.9f, true);

            swell (synth, 0.6f, 2.0f, 0.9f);
        } },

        // A pitch that climbs for ever and gets nowhere: four sines an octave apart all rise an octave every 20
        // seconds, the lowest fading in as the highest fades out, each on an LFO drawn for it, so when they drop back
        // an octave together nothing's heard to change. A wind of noise rises with them. Nothing's random.
        { "Infinite Staircase", [] (SynthProcessor& synth)
        {
            setLfo (synth, lfo1, 20.0f, loops, waveform::Preset::sawtooth);

            for (int i = 0; i < 4; ++i)
            {
                // Each climbs the 12 semitones round its tune, and is as loud as a bell curve over all four octaves
                // makes its pitch.
                const auto tune = -18.0 + 12.0 * i;
                setPartials (synth, i, 0.5f, (float) tune, { 1.0f, 0.0f, 0.06f });
                setUnison (synth, i, 3, 7.0f, 0.8f);
                assign (synth, oscillatorTuneTarget (i), lfo1, 0.25f);

                setLfo (synth, lfo2 + i, 20.0f, loops, draw ([tune] (double t)
                {
                    const auto semitones = tune + 12.0 * (t - 0.5);
                    return 2.0 * std::exp (-std::pow (semitones / 13.0, 2.0)) - 1.0;
                }));
                assign (synth, oscillatorLevelTarget (i), lfo2 + i, 1.0f);
                setSends (synth, i, {});
            }

            setNoise (synth, osc5, 0.25f);
            setFilter (synth, 0, FilterType::lowPass, 900.0f, 0.7f);
            assign (synth, filterCutoffTarget (0), lfo1, 0.3f);
            slow (synth, lfo6, 7.0f, sine);
            assign (synth, oscillatorPanTarget (osc5), lfo6, 0.8f);

            swell (synth, 1.5f, 3.0f, 0.66f);
        } },

        // Two tones whose levels LFOs shake tens of times a second, fast enough to be heard not as a tremolo but as
        // new pitches either side of each harmonic. A 20 second LFO moves one's rate and a wandering one the other's,
        // so those pitches slide through each other, in tune with nothing.
        { "Ring Fog", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.5f, 0.0f, { 1.0f, 0.5f, 0.3f, 0.2f });
            setPartials (synth, osc2, 0.5f, 7.02f, { 1.0f, 0.4f, 0.2f });
            setPartials (synth, osc3, 0.4f, -12.0f, { 1.0f });
            setPan (synth, osc1, 0.3f);
            setPan (synth, osc2, 0.7f);

            slow (synth, lfo1, 1.0f / 35.0f, sine);
            assign (synth, oscillatorLevelTarget (osc1), lfo1, 1.0f);
            slow (synth, lfo2, 20.0f, sine);
            assign (synth, lfoRateTarget (lfo1), lfo2, 0.25f);

            slow (synth, lfo3, 1.0f / 50.0f, sine);
            assign (synth, oscillatorLevelTarget (osc2), lfo3, 1.0f);
            drift (synth, lfo4, 13.0f);
            assign (synth, lfoRateTarget (lfo3), lfo4, 0.25f);

            slow (synth, lfo5, 16.0f, triangle);
            assign (synth, oscillatorPanTarget (osc1), lfo5, 0.6f);
            assign (synth, oscillatorPanTarget (osc2), lfo5, 0.6f, true);

            setFilter (synth, 0, FilterType::lowPass, 3500.0f);
            assignModWheel (synth, lfoRateTarget (lfo1), 0.2f);

            swell (synth, 1.2f, 3.0f, 0.72f);
        } },

        // Two octaves below the keys: a sawtooth, a square and a sine whose tunes wander a third of a semitone apart,
        // grinding slowly, through a resonant low pass and a notch that sweep at their own speeds, over a rumble of
        // noise. A fifth swells up out of it every 20 seconds and sinks back. Legato notes slide for over a second.
        { "Tectonic Drone", [] (SynthProcessor& synth)
        {
            setOscillator (synth, osc1, fallingSawtooth(), 12, 0.7f);
            setTune (synth, osc1, -24.0f);
            setUnison (synth, osc1, 3, 6.0f, 0.6f);
            setOscillator (synth, osc2, square, 7, 0.5f);
            setTune (synth, osc2, -12.0f);
            setPartials (synth, osc3, 0.6f, -24.0f, { 1.0f });
            setSends (synth, osc3, {});
            setGlide (synth, { osc1, osc2, osc3, osc4 }, Glide::legato, 1.2f);

            setPartials (synth, osc4, 0.3f, -4.98f, { 1.0f, 0.5f, 0.3f, 0.2f, 0.1f });
            setLfo (synth, lfo1, 20.0f, loops, draw ([] (double t) { return 2.0 * std::pow (std::sin (pi * t), 4.0) - 1.0; }));
            assign (synth, oscillatorLevelTarget (osc4), lfo1, 0.6f);

            drift (synth, lfo2, 9.0f);
            drift (synth, lfo3, 14.0f);
            setTuneReach (synth, osc1, 1.0);
            setTuneReach (synth, osc2, 1.0);
            assign (synth, oscillatorTuneTarget (osc1), lfo2, 0.3f);
            assign (synth, oscillatorTuneTarget (osc2), lfo3, 0.3f);

            setFilter (synth, 0, FilterType::lowPass, 500.0f, 0.5f);
            slow (synth, lfo4, 16.0f, triangle);
            assign (synth, filterCutoffTarget (0), lfo4, 0.3f);
            assignModWheel (synth, filterCutoffTarget (0), 0.3f);

            setFilter (synth, 3, FilterType::notch, 300.0f);
            setParameter (synth, SynthProcessor::filterIds (3).q, 4.0f);
            slow (synth, lfo5, 7.0f, sine);
            assign (synth, filterCutoffTarget (3), lfo5, 0.3f);

            setNoise (synth, osc5, 0.3f);
            setSends (synth, osc5, { 0 });
            drift (synth, lfo6, 3.0f);
            assign (synth, oscillatorLevelTarget (osc5), lfo6, 0.5f);

            swell (synth, 2.5f, 5.0f, 0.6f);
        } },

        // The overtone series comes apart: an 18 second LFO tunes every harmonic of a sawtooth away from its place,
        // the higher the further, stretching the series wide, then squeezing it, with one clear moment in tune as it
        // passes through. An oscillator an octave down does the opposite, every 13 seconds. Nothing's random.
        { "Melting Harmonics", [] (SynthProcessor& synth)
        {
            std::vector<float> falling;

            for (int harmonic = 1; harmonic <= 16; ++harmonic)
                falling.push_back (1.0f / (float) harmonic);

            setPartials (synth, osc1, 0.7f, 0.0f, falling);
            setPartials (synth, osc2, 0.5f, -12.0f, falling);
            setPan (synth, osc1, 0.35f);
            setPan (synth, osc2, 0.65f);

            slow (synth, lfo1, 18.0f, sine);
            slow (synth, lfo2, 13.0f, sine);

            for (int harmonic = 2; harmonic <= 16; ++harmonic)
            {
                const auto stretch = 0.5f * (float) (harmonic - 1) / 15.0f;
                assign (synth, harmonicTuneTarget (osc1, harmonic), lfo1, stretch);
                assign (synth, harmonicTuneTarget (osc2, harmonic), lfo2, stretch, true);
            }

            setFilter (synth, 0, FilterType::lowPass, 3500.0f, 0.1f);
            assignVelocity (synth, filterCutoffTarget (0), 0.2f);
            assignModWheel (synth, filterCutoffTarget (0), 0.25f);

            swell (synth, 1.0f, 3.0f, 0.7f);
        } },

        // A chord whose four notes pulse in time with the tempo, each on steps drawn for it and each to a bar of its
        // own length, 3, 4, 6 and 8 beats, so the pattern only comes round every 24. Random LFOs, in time too, move
        // the low pass and throw the notes from side to side.
        { "Pulse Nebula", [] (SynthProcessor& synth)
        {
            constexpr std::array tunes { 0.0f, 7.02f, 12.0f, 15.86f };
            constexpr std::array rates { 0.25, 1.0 / 3.0, 1.0 / 6.0, 0.125 };
            const std::array<std::vector<double>, 4> steps { {
                { 1.0, -1.0, 0.2, -1.0, 0.6, -0.4, -1.0, 0.0 },
                { 0.8, -1.0, -0.2, 1.0, -1.0, 0.2 },
                { -1.0, 0.6, -1.0, -1.0, 1.0, -0.4, 0.0, -1.0, 0.4, -1.0, 0.8, -0.6 },
                { 1.0, 0.4, -0.2, -1.0, -1.0, 0.6, -1.0, 0.2, -1.0, -1.0, 0.8, 0.0, -1.0, 0.4, -1.0, -0.4 } } };

            for (int i = 0; i < 4; ++i)
            {
                setPartials (synth, i, 0.5f, tunes[(size_t) i], { 1.0f, 0.5f, 0.35f, 0.25f, 0.2f, 0.15f, 0.1f, 0.08f });
                setLfo (synth, i, 2.0f, loops, sine);
                setSteps (synth, i, steps[(size_t) i]);
                setSync (synth, i, rates[(size_t) i]);
                assign (synth, oscillatorLevelTarget (i), i, 1.0f);
            }

            setPartials (synth, osc5, 0.4f, -12.0f, { 1.0f, 0.3f });
            setSends (synth, osc5, {});

            setFilter (synth, 0, FilterType::lowPass, 1800.0f, 0.4f);
            drift (synth, lfo5, 8.0f);
            setSync (synth, lfo5, 0.0625);
            assign (synth, filterCutoffTarget (0), lfo5, 0.4f);

            jumps (synth, lfo6, 4.0f, 8);
            setSync (synth, lfo6, 0.125);

            for (int i = 0; i < 4; ++i)
                assign (synth, oscillatorPanTarget (i), lfo6, 0.9f, i % 2 == 1, (float) i / 4.0f);

            swell (synth, 0.5f, 2.0f, 0.62f);
        } },

        // A machine that rebuilds itself as it runs: wandering LFOs switch three of its oscillators, a mid pass and
        // a notch in and out as they cross their thresholds, and twice in 20 seconds its first oscillator's harmonics
        // are randomized, and now and then its third's. It's never left as it was found.
        { "Ghost Machine", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.5f, 0.0f, { 1.0f, 0.5f, 0.3f, 0.4f, 0.2f, 0.25f, 0.1f, 0.15f, 0.1f, 0.2f, 0.05f, 0.1f });
            setOscillator (synth, osc2, square, 9, 0.35f);
            setTune (synth, osc2, 7.02f);
            setPartials (synth, osc3, 0.35f, 12.0f, { 1.0f, 0.3f, 0.4f, 0.2f, 0.1f, 0.2f });
            setNoise (synth, osc4, 0.15f);
            setPartials (synth, osc5, 0.5f, -12.0f, { 1.0f, 0.2f });
            setPan (synth, osc2, 0.2f);
            setPan (synth, osc3, 0.8f);

            drift (synth, lfo1, 7.0f);
            trigger (synth, oscillatorOnTarget (osc2), lfo1, 0.5f);
            drift (synth, lfo2, 11.0f);
            trigger (synth, oscillatorOnTarget (osc3), lfo2, 0.4f);
            trigger (synth, randomizeTarget (osc3), lfo2, 0.7f);
            drift (synth, lfo3, 5.0f);
            trigger (synth, oscillatorOnTarget (osc4), lfo3, 0.6f);

            setFilter (synth, 2, FilterType::midPass, 1000.0f);
            setParameter (synth, SynthProcessor::filterIds (2).width, 0.8f);
            setFilter (synth, 3, FilterType::notch, 2000.0f);
            setSends (synth, osc5, { 0 });
            drift (synth, lfo4, 13.0f);
            trigger (synth, filterOnTarget (2), lfo4, 0.5f);
            trigger (synth, filterOnTarget (3), lfo4, 0.3f);

            slow (synth, lfo5, 20.0f, sine);
            trigger (synth, randomizeTarget (osc1), lfo5, 0.5f);

            setFilter (synth, 0, FilterType::lowPass, 2500.0f, 0.3f);
            slow (synth, lfo6, 17.0f, triangle);
            assign (synth, filterCutoffTarget (0), lfo6, 0.3f);
            assign (synth, filterCutoffTarget (2), lfo6, 0.3f, true);

            swell (synth, 1.0f, 3.0f, 0.85f);
        } },

        // A tape left in the sun: the pitch wows and flutters on wandering LFOs, the low pass comes and goes, the
        // level drops out in random steps, and hiss, sent only through the high pass, plays over it. Each note starts
        // flat and comes up to speed.
        { "Warped Tape", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.8f, 0.0f, { 1.0f, 0.4f, 0.15f, 0.08f, 0.04f });
            setPartials (synth, osc2, 0.4f, 12.03f, { 1.0f, 0.2f, 0.1f });
            setUnison (synth, osc1, 2, 10.0f, 0.5f);
            setSends (synth, osc1, { 0 });
            setSends (synth, osc2, { 0 });

            drift (synth, lfo1, 2.5f);
            setTuneReach (synth, osc1, 1.0);
            assign (synth, oscillatorTuneTarget (osc1), lfo1, 0.35f);
            assign (synth, oscillatorPanTarget (osc1), lfo1, 0.3f);
            drift (synth, lfo2, 0.12f);
            setTuneReach (synth, osc2, 1.0);
            assign (synth, oscillatorTuneTarget (osc2), lfo2, 0.12f);

            setFilter (synth, 0, FilterType::lowPass, 2200.0f, 0.15f);
            drift (synth, lfo3, 4.0f);
            assign (synth, filterCutoffTarget (0), lfo3, 0.25f);

            jumps (synth, lfo4, 10.0f, 16);
            setParameter (synth, SynthProcessor::levelId (osc1), 0.7f);
            assign (synth, oscillatorLevelTarget (osc1), lfo4, 0.6f);

            drift (synth, lfo5, 20.0f);
            assign (synth, unisonDetuneTarget (osc1), lfo5, 0.4f);

            setNoise (synth, osc3, 0.04f);
            setSends (synth, osc3, { 1 });
            setFilter (synth, 1, FilterType::highPass, 3000.0f);

            swell (synth, 0.05f, 1.5f, 0.75f);
        } },

        // Metal that won't set: ten partials, none where a harmonic belongs, each sliding towards its neighbours and
        // back on one of four wandering LFOs, from 6 to 20 seconds long, so the bell it might have been never forms.
        // A mid pass narrows and widens over it and wanders up and down.
        { "Liquid Metal", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.9f, 0.0f, { 1.0f, 0.6f, 0.5f, 0.4f, 0.35f, 0.3f, 0.25f, 0.2f, 0.18f, 0.15f });
            setPartials (synth, osc2, 0.7f, 12.0f, { 0.0f, 0.6f, 0.5f, 0.4f, 0.3f, 0.25f, 0.2f });
            setUnison (synth, osc2, 2, 15.0f, 1.0f);
            setPan (synth, osc1, 0.4f);

            constexpr std::array tunes { 0.31f, -0.2f, 0.42f, -0.35f, 0.15f, -0.45f, 0.27f, -0.12f, 0.38f };
            constexpr std::array periods { 6.0f, 9.0f, 14.0f, 20.0f };

            for (int lfo = 0; lfo < 4; ++lfo)
                drift (synth, lfo, periods[(size_t) lfo]);

            for (int harmonic = 2; harmonic <= 10; ++harmonic)
            {
                synth.setHarmonicTune (osc1, harmonic, tunes[(size_t) (harmonic - 2)]);
                assign (synth, harmonicTuneTarget (osc1, harmonic), harmonic % 4, 0.5f, harmonic % 3 == 0, (float) harmonic / 10.0f);
            }

            for (int harmonic = 2; harmonic <= 7; ++harmonic)
            {
                synth.setHarmonicTune (osc2, harmonic, -tunes[(size_t) (harmonic - 2)]);
                assign (synth, harmonicTuneTarget (osc2, harmonic), (harmonic + 2) % 4, 0.6f, harmonic % 2 == 0, (float) harmonic / 7.0f);
            }

            setFilter (synth, 2, FilterType::midPass, 1200.0f);
            setParameter (synth, SynthProcessor::filterIds (2).width, 3.0f);
            slow (synth, lfo5, 16.0f, sine);
            assign (synth, filterWidthTarget (2), lfo5, 0.5f);
            drift (synth, lfo6, 8.0f);
            assign (synth, filterCutoffTarget (2), lfo6, 0.3f);
            assign (synth, oscillatorPanTarget (osc1), lfo6, 0.5f);

            swell (synth, 0.6f, 4.0f, 1.0f);
        } },

        // Something calling in deep water: every 17 seconds from the key a voice swoops up a fifth and moans back
        // down past where it started, its formant opening as it rises, and every 20 another, an octave below, groans
        // downwards. A wandering LFO hurries and holds them back, so the calls don't keep time, over a sub and a wash
        // of noise. Legato notes slide for over a second.
        { "Whale Canyon", [] (SynthProcessor& synth)
        {
            setPartials (synth, osc1, 0.7f, 0.0f, { 1.0f, 0.5f, 0.1f, 0.2f, 0.05f });
            setPartials (synth, osc2, 0.5f, -12.0f, { 1.0f, 0.6f, 0.3f, 0.2f, 0.1f, 0.1f });
            setUnison (synth, osc1, 2, 12.0f, 0.6f);
            setGlide (synth, { osc1, osc2 }, Glide::legato, 1.2f);

            setLfo (synth, lfo1, 17.0f, fromKey, draw ([] (double t)
            {
                return t < 0.5 ? 0.0 : t < 0.6 ? ease ((t - 0.5) / 0.1) : t < 0.9 ? 1.0 - 1.3 * ease ((t - 0.6) / 0.3) : -0.3 * (1.0 - ease ((t - 0.9) / 0.1));
            }));
            assign (synth, oscillatorTuneTarget (osc1), lfo1, 0.3f);

            setLfo (synth, lfo2, 20.0f, fromKey, draw ([] (double t)
            {
                return t < 0.2 ? 0.0 : t < 0.5 ? -ease ((t - 0.2) / 0.3) : t < 0.6 ? -1.0 + 1.4 * ease ((t - 0.5) / 0.1) : 0.4 * (1.0 - ease ((t - 0.6) / 0.4));
            }));
            assign (synth, oscillatorTuneTarget (osc2), lfo2, 0.2f);

            drift (synth, lfo3, 10.0f);
            assign (synth, lfoRateTarget (lfo1), lfo3, 0.1f);
            assign (synth, lfoRateTarget (lfo2), lfo3, 0.1f, true);

            setFilter (synth, 0, FilterType::lowPass, 700.0f, 0.5f);
            assign (synth, filterCutoffTarget (0), lfo1, 0.25f);

            setPartials (synth, osc3, 0.25f, -24.0f, { 1.0f });
            setSends (synth, osc3, {});

            setNoise (synth, osc4, 0.1f);
            drift (synth, lfo4, 6.0f);
            assign (synth, oscillatorLevelTarget (osc4), lfo4, 0.2f);

            slow (synth, lfo5, 20.0f, sine);
            assign (synth, oscillatorPanTarget (osc1), lfo5, 0.7f);
            assign (synth, oscillatorPanTarget (osc2), lfo5, 0.5f, true);

            swell (synth, 2.0f, 5.0f, 0.66f);
        } },

        // An organ in a building that changes size: four ranks, each playing more of its harmonics and then fewer on
        // an LFO of its own, 13, 17, 19 and 20 seconds long, so the brightness of each comes and goes like a tide,
        // while they turn slowly round each other from side to side. Nothing's random.
        { "Tidal Cathedral", [] (SynthProcessor& synth)
        {
            constexpr std::array tunes { 0.0f, 7.02f, 12.0f, -12.0f };
            constexpr std::array levels { 0.6f, 0.4f, 0.35f, 0.55f };
            constexpr std::array periods { 20.0f, 17.0f, 13.0f, 19.0f };

            for (int i = 0; i < 4; ++i)
            {
                setPartials (synth, i, levels[(size_t) i], tunes[(size_t) i], organ);
                setParameter (synth, SynthProcessor::harmonicsId (i), 8.0f);
                slow (synth, i, periods[(size_t) i], i % 2 == 0 ? sine : triangle);
                assign (synth, oscillatorHarmonicsTarget (i), i, 0.9f);
                assign (synth, oscillatorPanTarget (i), (i + 1) % 4, 0.7f, false, 0.25f);
            }

            setUnison (synth, osc1, 3, 8.0f, 0.6f);

            setFilter (synth, 0, FilterType::lowPass, 2500.0f, 0.2f);
            assign (synth, filterCutoffTarget (0), lfo4, 0.2f, false, 0.5f);
            assignModWheel (synth, filterCutoffTarget (0), 0.25f);

            swell (synth, 2.5f, 4.0f, 0.72f);
        } },
    };
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    const auto folder = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::CharPointer_UTF8 (argv[1]))
                                 : SynthProcessor::getPresetFolder();
    auto failed = false;

    for (const auto& recipe : recipes)
    {
        const auto file = folder.getChildFile (recipe.name).withFileExtension (SynthProcessor::presetExtension);

        // One of yours under the same name is left alone.
        if (file.existsAsFile() && SynthProcessor::getPresetLabel (file) != label)
        {
            std::cout << "Left " << file.getFullPathName() << ", which isn't one of these" << std::endl;
            failed = true;
            continue;
        }

        SynthProcessor synth;
        recipe.make (synth);

        const auto saved = synth.savePreset (file, category, label);
        failed = failed || ! saved;
        std::cout << (saved ? "Saved " : "Couldn't save ") << file.getFullPathName() << std::endl;
    }

    return failed ? 1 : 0;
}
