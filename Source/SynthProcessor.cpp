#include "SynthProcessor.h"

#include "SynthEditor.h"

namespace
{
    constexpr int numVoices = 16;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { SynthProcessor::volumeId, 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })
                .withValueFromStringFunction ([] (const juce::String& text) { return text.retainCharacters ("0123456789.").getFloatValue() / 100.0f; })));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { SynthProcessor::harmonicsId, 1 }, "Harmonics", 1, waveform::maxPartials, waveform::defaultPartials));

        return layout;
    }

    /** The volume knob follows a square law, which sounds more even over its turn than a straight line does. */
    float volumeToGain (float volume)
    {
        return volume * volume;
    }
}

SynthProcessor::SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "EZSynth", createParameters())
{
    volume = parameters.getRawParameterValue (volumeId);
    numPartials = parameters.getRawParameterValue (harmonicsId);

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new HarmonicVoice (targets));

    synth.addSound (new HarmonicSound());

    setShape (waveform::makePreset (waveform::Preset::sawtooth), waveform::Preset::sawtooth);
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

    if (sharedVersion.load() != playingVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (sharedLock);

        if (lock.isLocked())
        {
            playingDecomposition = sharedDecomposition;
            playingVersion = sharedVersion.load();
            playingNumPartials = -1;
        }
    }

    if (getNumPartials() != playingNumPartials)
        updateTargets();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    gain.setTargetValue (volumeToGain (volume->load()));
    gain.applyGain (buffer, buffer.getNumSamples());
}

void SynthProcessor::updateTargets()
{
    playingNumPartials = getNumPartials();

    // Every wave is played at the same peak level, however many sine waves it has.
    const auto used = playingDecomposition.getNumUsed (playingNumPartials);
    const auto peak = playingDecomposition.peaks[(size_t) used];
    const auto scale = peak > 1.0e-6f ? 1.0f / peak : 0.0f;

    targets.cosines.fill (0.0f);
    targets.sines.fill (0.0f);

    // A sin (θ + φ) = A sin φ cos θ + A cos φ sin θ
    for (int i = 0; i < used; ++i)
    {
        const auto& partial = playingDecomposition.strongest[(size_t) i];
        targets.cosines[(size_t) partial.harmonic] = scale * partial.amplitude * std::sin (partial.phase);
        targets.sines[(size_t) partial.harmonic] = scale * partial.amplitude * std::cos (partial.phase);
    }

    ++targets.version;
}

int SynthProcessor::getNumPartials() const noexcept
{
    return juce::roundToInt (numPartials->load());
}

void SynthProcessor::setShape (const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    shape = newShape;
    preset = fromPreset;
    decomposition = waveform::decompose (shape);

    {
        const juce::SpinLock::ScopedLockType lock (sharedLock);
        sharedDecomposition = decomposition;
        ++sharedVersion;
    }

    ++shapeVersion;
}

void SynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    juce::ValueTree wave ("WAVE");
    wave.setProperty ("points", juce::MemoryBlock (shape.data(), sizeof (shape)).toBase64Encoding(), nullptr);
    wave.setProperty ("preset", preset.has_value() ? waveform::getName (*preset) : juce::String(), nullptr);
    state.appendChild (wave, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto wave = state.getChildWithName ("WAVE");
    state.removeChild (wave, nullptr);
    parameters.replaceState (state);

    juce::MemoryBlock points;

    if (points.fromBase64Encoding (wave["points"].toString()) && points.getSize() == sizeof (waveform::Shape))
    {
        waveform::Shape restored {};
        std::memcpy (restored.data(), points.getData(), sizeof (restored));

        for (auto& value : restored)
            value = std::isfinite (value) ? juce::jlimit (-1.0f, 1.0f, value) : 0.0f;

        setShape (restored, waveform::findPreset (wave["preset"].toString()));
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
