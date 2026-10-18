#include "SynthProcessor.h"

#include "SynthEditor.h"

namespace
{
    constexpr int numVoices = 16;

    /** The waves the oscillators start with, so each sounds different when it's switched on. */
    constexpr std::array<waveform::Preset, SynthProcessor::numOscillators> startingPresets {
        waveform::Preset::sawtooth, waveform::Preset::square, waveform::Preset::sine
    };

    juce::AudioParameterFloatAttributes asPercentage()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return text.retainCharacters ("0123456789.").getFloatValue() / 100.0f; });
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { SynthProcessor::volumeId, 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f,
            asPercentage()));

        // Only the first oscillator plays to begin with.
        for (int i = 0; i < SynthProcessor::numOscillators; ++i)
        {
            const auto name = "Osc " + juce::String (i + 1);

            layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
                "osc" + juce::String (i + 1), name, " ",
                std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { SynthProcessor::onId (i), 1 }, name + " On", i == 0),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::levelId (i), 1 }, name + " Level",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, asPercentage()),
                std::make_unique<juce::AudioParameterInt> (
                    juce::ParameterID { SynthProcessor::harmonicsId (i), 1 }, name + " Harmonics",
                    1, waveform::maxPartials, waveform::defaultPartials)));
        }

        return layout;
    }

    /** The volume knob follows a square law, which sounds more even over its turn than a straight line does. */
    float volumeToGain (float volume)
    {
        return volume * volume;
    }
}

juce::String SynthProcessor::onId (int oscillator)          { return "osc" + juce::String (oscillator + 1) + "On"; }
juce::String SynthProcessor::levelId (int oscillator)       { return "osc" + juce::String (oscillator + 1) + "Level"; }
juce::String SynthProcessor::harmonicsId (int oscillator)   { return "osc" + juce::String (oscillator + 1) + "Harmonics"; }

SynthProcessor::SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "EZSynth", createParameters())
{
    volume = parameters.getRawParameterValue (volumeId);

    for (int i = 0; i < numOscillators; ++i)
        oscillatorParameters[(size_t) i] = { parameters.getRawParameterValue (onId (i)),
                                             parameters.getRawParameterValue (levelId (i)),
                                             parameters.getRawParameterValue (harmonicsId (i)) };

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new HarmonicVoice (targets));

    synth.addSound (new HarmonicSound());

    for (int i = 0; i < numOscillators; ++i)
        setShape (i, waveform::makePreset (startingPresets[(size_t) i]), startingPresets[(size_t) i]);
}

const juce::String SynthProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void SynthProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    gain.reset (sampleRate, 0.05);
    gain.setCurrentAndTargetValue (volumeToGain (volume->load()));
}

void SynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Adds the notes played on the on-screen keyboard, and lights its keys for notes played from elsewhere.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midi)
        if (const auto message = metadata.getMessage(); message.isNoteOn())
            lastNote = message.getNoteNumber();

    auto changed = false;

    if (sharedVersion.load() != playingVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (sharedLock);

        if (lock.isLocked())
        {
            playingDecompositions = sharedDecompositions;
            playingVersion = sharedVersion.load();
            changed = true;
        }
    }

    for (int i = 0; i < numOscillators; ++i)
    {
        if (const auto settings = readSettings (i); settings != playingSettings[(size_t) i])
        {
            playingSettings[(size_t) i] = settings;
            changed = true;
        }
    }

    if (changed)
        updateTargets();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    gain.setTargetValue (volumeToGain (volume->load()));
    gain.applyGain (buffer, buffer.getNumSamples());
}

SynthProcessor::Settings SynthProcessor::readSettings (int oscillator) const noexcept
{
    const auto& oscillatorParameter = oscillatorParameters[(size_t) oscillator];
    return { oscillatorParameter.on->load() >= 0.5f,
             oscillatorParameter.level->load(),
             juce::roundToInt (oscillatorParameter.numPartials->load()) };
}

void SynthProcessor::updateTargets()
{
    targets.cosines.fill (0.0f);
    targets.sines.fill (0.0f);

    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        const auto& settings = playingSettings[i];
        const auto& decomposition = playingDecompositions[i];

        if (! settings.on)
            continue;

        // Each oscillator's wave peaks at its level, however many sine waves it has.
        const auto used = decomposition.getNumUsed (settings.numPartials);
        const auto peak = decomposition.peaks[(size_t) used];
        const auto scale = peak > 1.0e-6f ? settings.level / peak : 0.0f;

        // A sin (θ + φ) = A sin φ cos θ + A cos φ sin θ. Every oscillator plays the same note, so the sine waves
        // at each harmonic add up to one, and the voices play all the oscillators at once.
        for (int j = 0; j < used; ++j)
        {
            const auto& partial = decomposition.strongest[(size_t) j];
            targets.cosines[(size_t) partial.harmonic] += scale * partial.amplitude * std::sin (partial.phase);
            targets.sines[(size_t) partial.harmonic] += scale * partial.amplitude * std::cos (partial.phase);
        }
    }

    ++targets.version;
}

const waveform::Shape& SynthProcessor::getShape (int oscillator) const noexcept
{
    return oscillators[(size_t) oscillator].shape;
}

const waveform::Decomposition& SynthProcessor::getDecomposition (int oscillator) const noexcept
{
    return oscillators[(size_t) oscillator].decomposition;
}

std::optional<waveform::Preset> SynthProcessor::getPreset (int oscillator) const noexcept
{
    return oscillators[(size_t) oscillator].preset;
}

bool SynthProcessor::isOn (int oscillator) const noexcept
{
    return oscillatorParameters[(size_t) oscillator].on->load() >= 0.5f;
}

int SynthProcessor::getNumPartials (int oscillator) const noexcept
{
    return juce::roundToInt (oscillatorParameters[(size_t) oscillator].numPartials->load());
}

void SynthProcessor::setShape (int oscillator, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    auto& changed = oscillators[(size_t) oscillator];
    changed.shape = newShape;
    changed.preset = fromPreset;
    changed.decomposition = waveform::decompose (newShape);

    {
        const juce::SpinLock::ScopedLockType lock (sharedLock);
        sharedDecompositions[(size_t) oscillator] = changed.decomposition;
        ++sharedVersion;
    }

    ++shapeVersion;
}

void SynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    for (int i = 0; i < numOscillators; ++i)
    {
        const auto& oscillator = oscillators[(size_t) i];

        juce::ValueTree wave ("WAVE");
        wave.setProperty ("oscillator", i, nullptr);
        wave.setProperty ("points", juce::MemoryBlock (oscillator.shape.data(), sizeof (oscillator.shape)).toBase64Encoding(), nullptr);
        wave.setProperty ("preset", oscillator.preset.has_value() ? waveform::getName (*oscillator.preset) : juce::String(), nullptr);
        state.appendChild (wave, nullptr);
    }

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    std::vector<juce::ValueTree> waves;

    for (auto wave = state.getChildWithName ("WAVE"); wave.isValid(); wave = state.getChildWithName ("WAVE"))
    {
        waves.push_back (wave);
        state.removeChild (wave, nullptr);
    }

    parameters.replaceState (state);

    for (const auto& wave : waves)
    {
        // Saves from before there were three oscillators have one wave, with no number.
        const auto oscillator = (int) wave.getProperty ("oscillator", 0);
        juce::MemoryBlock points;

        if (oscillator < 0 || oscillator >= numOscillators
            || ! points.fromBase64Encoding (wave["points"].toString()) || points.getSize() != sizeof (waveform::Shape))
            continue;

        waveform::Shape restored {};
        std::memcpy (restored.data(), points.getData(), sizeof (restored));

        for (auto& value : restored)
            value = std::isfinite (value) ? juce::jlimit (-1.0f, 1.0f, value) : 0.0f;

        setShape (oscillator, restored, waveform::findPreset (wave["preset"].toString()));
    }
}

juce::AudioProcessorEditor* SynthProcessor::createEditor()
{
    return new SynthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthProcessor();
}
