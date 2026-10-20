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
    void setHarmonics (SynthProcessor& synth, int oscillator, std::initializer_list<float> amplitudes)
    {
        auto harmonic = 1;

        for (const auto amplitude : amplitudes)
            synth.setHarmonicAmplitude (oscillator, harmonic++, amplitude);
    }

    void setFilter (SynthProcessor& synth, const SynthProcessor::FilterIds& ids, float cutoff, float resonance)
    {
        setParameter (synth, ids.on, 1.0f);
        setParameter (synth, ids.cutoff, cutoff);
        setParameter (synth, ids.resonance, resonance);
    }

    void setEnvelope (SynthProcessor& synth, int envelope, const EnvelopeSettings& settings)
    {
        setParameter (synth, SynthProcessor::attackId (envelope), settings.attack);
        setParameter (synth, SynthProcessor::decayId (envelope), settings.decay);
        setParameter (synth, SynthProcessor::sustainId (envelope), settings.sustain);
        setParameter (synth, SynthProcessor::releaseId (envelope), settings.release);
    }

    void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, bool once, const waveform::Shape& shape,
                 std::optional<waveform::Preset> preset = std::nullopt)
    {
        setParameter (synth, SynthProcessor::lfoPeriodId (lfo), periodSeconds);
        setParameter (synth, SynthProcessor::lfoModeId (lfo), once ? 1.0f : 0.0f);
        synth.setLfoShape (lfo, shape, preset);
    }

    void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, bool once, waveform::Preset preset)
    {
        setLfo (synth, lfo, periodSeconds, once, waveform::makePreset (preset), preset);
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

    void assign (SynthProcessor& synth, int target, int source, float range, bool inverted = false)
    {
        synth.setAssignment (target, { source, range, inverted });
    }

    void assignVelocity (SynthProcessor& synth, int target, float range)
    {
        synth.setVelocityAssignment (target, { modulation::velocitySource, range, false });
    }

    using namespace modulation;

    constexpr int lfo1 = 0, lfo2 = 1, lfo3 = 2;
    constexpr int env1 = envelopeSource (0), env2 = envelopeSource (1), env3 = envelopeSource (2);

    struct Recipe
    {
        const char* name;
        std::function<void (SynthProcessor&)> make;
    };

    const std::vector<Recipe> recipes {
        // A wave drawn from its harmonics, a strike that lights up the 9th harmonic for a moment, and a long ring
        // shaped by an envelope on the volume.
        { "Bell", [] (SynthProcessor& synth)
        {
            synth.setShape (0, {});
            setParameter (synth, SynthProcessor::harmonicsId (0), 9.0f);
            setHarmonics (synth, 0, { 0.7f, 0.0f, 0.25f, 0.5f, 0.0f, 0.2f, 0.3f });
            setOscillator (synth, 1, waveform::Preset::sine, 1, 0.3f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 1, { 0.0f, 4.0f, 0.0f, 4.0f });
            assign (synth, volumeTarget, env2, 0.7f);

            setEnvelope (synth, 0, { 0.0f, 0.5f, 0.0f, 0.5f });
            assign (synth, harmonicTarget (0, 9), env1, 0.55f);
        } },

        // A sawtooth through a low pass filter that an envelope opens with each note, and that softer notes open less.
        { "Brass", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::sawtooth, 9, 1.0f);
            setParameter (synth, SynthProcessor::volumeId, 0.8f);

            setFilter (synth, SynthProcessor::filterIds (0), 500.0f, 0.25f);
            setEnvelope (synth, 0, { 0.06f, 0.5f, 0.45f, 0.3f });
            assign (synth, filterCutoffTarget (0), env1, 0.35f);
            assignVelocity (synth, filterCutoffTarget (0), 0.25f);
        } },

        // Velocity at work alongside the envelopes: softer notes are quieter, darker and lose the triangle's bark,
        // while the volume dies away and a quick envelope on the 4th harmonic adds a tine's ping.
        { "Electric Keys", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::sine, 6, 1.0f);
            setHarmonics (synth, 0, { 1.0f, 0.25f, 0.08f });
            setOscillator (synth, 1, waveform::Preset::triangle, 5, 0.45f);
            assignVelocity (synth, oscillatorLevelTarget (1), 0.45f);

            setFilter (synth, SynthProcessor::filterIds (0), 3500.0f, 0.1f);
            assignVelocity (synth, filterCutoffTarget (0), 0.35f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 0, { 0.005f, 3.0f, 0.3f, 0.5f });
            assign (synth, volumeTarget, env1, 0.65f);

            setEnvelope (synth, 1, { 0.0f, 0.25f, 0.0f, 0.2f });
            assign (synth, harmonicTarget (0, 4), env2, 0.5f);
        } },

        // One LFO moving two controls at once: it plays a sawtooth, drawn falling, with more harmonics as it plays a
        // square with fewer, and back, over a steady sine.
        { "Harmonic Sweep", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, fallingSawtooth(), 5, 1.0f);
            setOscillator (synth, 1, waveform::Preset::square, 5, 0.6f);
            setOscillator (synth, 2, waveform::Preset::sine, 1, 0.5f);
            setParameter (synth, SynthProcessor::volumeId, 0.5f);

            setLfo (synth, lfo1, 4.0f, false, waveform::Preset::triangle);
            assign (synth, oscillatorHarmonicsTarget (0), lfo1, 1.0f);
            assign (synth, oscillatorHarmonicsTarget (1), lfo1, 1.0f, true);
        } },

        // A sine with raised harmonics, like an organ's drawbars, with a quick tremolo and a click as each key goes
        // down. Velocity is taken off the volume, as an organ plays every note as loud.
        { "Organ", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::sine, 9, 1.0f);
            setHarmonics (synth, 0, { 1.0f, 0.6f, 0.45f, 0.3f, 0.0f, 0.2f, 0.0f, 0.15f });
            synth.setVelocityAssignment (volumeTarget, {});
            setParameter (synth, SynthProcessor::volumeId, 0.75f);

            setLfo (synth, lfo1, 0.16f, false, waveform::Preset::sine);
            assign (synth, volumeTarget, lfo1, 0.06f);

            setEnvelope (synth, 1, { 0.0f, 0.04f, 0.0f, 0.04f });
            assign (synth, harmonicTarget (0, 9), env2, 0.4f);
        } },

        // An oscillator at no level that an envelope plucks, with a low pass filter that snaps shut after it.
        { "Pluck", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::sawtooth, 9, 0.0f);
            setEnvelope (synth, 1, { 0.0f, 0.6f, 0.0f, 0.3f });
            assign (synth, oscillatorLevelTarget (0), env2, 1.0f);
            setParameter (synth, SynthProcessor::volumeId, 0.9f);

            setFilter (synth, SynthProcessor::filterIds (0), 400.0f, 0.3f);
            setEnvelope (synth, 0, { 0.0f, 0.2f, 0.0f, 0.2f });
            assign (synth, filterCutoffTarget (0), env1, 0.45f);
            assignVelocity (synth, filterCutoffTarget (0), 0.3f);
        } },

        // All three oscillators, swelling in and fading slowly, through a low pass filter that one LFO slowly opens
        // and closes, while another makes a falling sawtooth shimmer.
        { "Soft Pad", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::triangle, 5, 0.8f);
            setOscillator (synth, 1, fallingSawtooth(), 9, 0.35f);
            setOscillator (synth, 2, waveform::Preset::sine, 1, 0.5f);

            setParameter (synth, SynthProcessor::volumeId, 0.0f);
            setEnvelope (synth, 2, { 1.5f, 1.0f, 0.85f, 2.5f });
            assign (synth, volumeTarget, env3, 0.6f);

            setFilter (synth, SynthProcessor::filterIds (0), 900.0f, 0.15f);
            setLfo (synth, lfo1, 6.0f, false, waveform::Preset::sine);
            assign (synth, filterCutoffTarget (0), lfo1, 0.3f);

            setLfo (synth, lfo2, 3.0f, false, waveform::Preset::triangle);
            assign (synth, oscillatorLevelTarget (1), lfo2, 0.5f);
        } },

        // A square between a high pass and a resonant low pass filter. An LFO that runs once per note, drawn to wait
        // and then rise, brings a falling sawtooth in under each note that's held.
        { "Square Lead", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::square, 9, 0.9f);
            setOscillator (synth, 1, fallingSawtooth(), 9, 0.15f);
            setParameter (synth, SynthProcessor::volumeId, 0.6f);

            setFilter (synth, SynthProcessor::filterIds (1), 180.0f, 0.0f);
            setFilter (synth, SynthProcessor::filterIds (0), 2800.0f, 0.35f);

            setLfo (synth, lfo3, 1.2f, true, draw ([] (double t)
            {
                return t < 0.35 ? -1.0 : -std::cos (pi * (t - 0.35) / 0.65);
            }));
            assign (synth, oscillatorLevelTarget (1), lfo3, 0.7f);
        } },

        // An LFO drawn as eight steps moves a resonant low pass filter's cutoff, like a step sequencer.
        { "Step Filter", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::sawtooth, 9, 1.0f);
            setParameter (synth, SynthProcessor::volumeId, 0.7f);

            setFilter (synth, SynthProcessor::filterIds (0), 700.0f, 0.55f);
            setLfo (synth, lfo2, 2.0f, false, draw ([] (double t)
            {
                constexpr std::array steps { -0.8, 0.6, -0.2, 1.0, -0.6, 0.3, -1.0, 0.8 };
                return steps[(size_t) (t * (double) steps.size())];
            }));
            synth.setDrawing (SynthProcessor::numOscillators + lfo2, { true, 8 });
            assign (synth, filterCutoffTarget (0), lfo2, 0.6f);
        } },

        // A square and a sine together through a resonant low pass filter that an LFO wobbles.
        { "Wobble Bass", [] (SynthProcessor& synth)
        {
            setOscillator (synth, 0, waveform::Preset::square, 9, 1.0f);
            setOscillator (synth, 2, waveform::Preset::sine, 1, 0.6f);
            setParameter (synth, SynthProcessor::volumeId, 0.5f);

            setFilter (synth, SynthProcessor::filterIds (0), 400.0f, 0.5f);
            setLfo (synth, lfo1, 0.25f, false, waveform::Preset::sine);
            assign (synth, filterCutoffTarget (0), lfo1, 0.5f);
        } },
    };
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "Presets");
    auto failed = false;

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
