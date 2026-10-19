#include "SynthProcessor.h"

#include "SynthEditor.h"

#include <BinaryData.h>

namespace
{
    constexpr int numVoices = 16;

    /** The waves the oscillators start with, so each sounds different when it's switched on. */
    constexpr std::array<waveform::Preset, SynthProcessor::numOscillators> startingPresets {
        waveform::Preset::sawtooth, waveform::Preset::square, waveform::Preset::sine
    };

    /** The waves and periods the LFOs start with. */
    constexpr std::array<waveform::Preset, SynthProcessor::numLfos> startingLfoPresets {
        waveform::Preset::sine, waveform::Preset::triangle, waveform::Preset::sawtooth
    };

    constexpr std::array<float, SynthProcessor::numLfos> startingPeriods { 1.0f, 2.0f, 4.0f };

    /** The envelopes start as three common shapes: a note that swells a little and settles, a pluck that dies
        away, and a slow swell.
    */
    constexpr std::array<EnvelopeSettings, SynthProcessor::numEnvelopes> startingEnvelopes { {
        { 0.01f, 0.3f, 0.7f, 0.5f },
        { 0.0f, 0.8f, 0.0f, 0.3f },
        { 2.0f, 1.0f, 1.0f, 2.0f }
    } };

    juce::AudioParameterFloatAttributes asPercentage()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return text.retainCharacters ("0123456789.").getFloatValue() / 100.0f; });
    }

    juce::String formatFrequency (float hertz)
    {
        // Compared once rounded, as a cutoff of 1 kHz comes back from its range as 999.99 Hz.
        if (juce::roundToInt (hertz) < 1000)
            return juce::String (juce::roundToInt (hertz)) + " Hz";

        return juce::String (hertz / 1000.0f, hertz < 10000.0f ? 2 : 1) + " kHz";
    }

    juce::String formatPeriod (float seconds)
    {
        return juce::String (seconds, seconds < 1.0f ? 2 : 1) + " s";
    }

    juce::String formatTime (float seconds)
    {
        // Compared once rounded, as a second comes back from its range as 999.99 ms.
        if (const auto milliseconds = juce::roundToInt (seconds * 1000.0f); milliseconds < 1000)
            return juce::String (milliseconds) + " ms";

        return juce::String (seconds, seconds < 10.0f ? 2 : 1) + " s";
    }

    /** An envelope's attack, decay or release, from 0 to 20 seconds, with the first second taking half the dial, as
        that's where most are set.
    */
    std::unique_ptr<juce::AudioParameterFloat> createTimeParameter (const juce::String& id, const juce::String& name,
                                                                    float defaultSeconds)
    {
        juce::NormalisableRange<float> range (0.0f, modulation::longestStage);
        range.setSkewForCentre (1.0f);

        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultSeconds,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float value, int) { return formatTime (value); })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    const auto number = text.retainCharacters ("0123456789.").getFloatValue();
                    return text.containsIgnoreCase ("ms") ? number / 1000.0f : number;
                }));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> createFilterParameters (const juce::String& groupId,
                                                                                const SynthProcessor::FilterIds& ids,
                                                                                const juce::String& name,
                                                                                float defaultCutoff)
    {
        // Cutoffs are spread evenly by octave, as pitch is heard.
        const juce::NormalisableRange<float> cutoffRange (FilterStage::lowestCutoff, FilterStage::highestCutoff,
                                                          [] (float, float, float position) { return FilterStage::positionToCutoff (position); },
                                                          [] (float, float, float hertz) { return FilterStage::cutoffToPosition (hertz); });

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            groupId, name, " ",
            std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids.on, 1 }, name + " On", false),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.cutoff, 1 }, name + " Cutoff", cutoffRange, defaultCutoff,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float value, int) { return formatFrequency (value); })
                    .withValueFromStringFunction ([] (const juce::String& text)
                    {
                        const auto number = text.retainCharacters ("0123456789.").getFloatValue();
                        return text.containsIgnoreCase ("k") ? number * 1000.0f : number;
                    })),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.resonance, 1 }, name + " Resonance", juce::NormalisableRange<float> (0.0f, 1.0f),
                0.0f, asPercentage()));
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
                    1, waveform::maxHarmonics, waveform::defaultHarmonics),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::tuneId (i), 1 }, name + " Tune",
                    juce::NormalisableRange<float> (0.0f, tuning::octave), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float semitones, int) { return tuning::toText (semitones); })
                        .withValueFromStringFunction ([] (const juce::String& text)
                        {
                            return text.retainCharacters ("0123456789.").getFloatValue();
                        }))));
        }

        // Both filters start switched out, at cutoffs that change the sound clearly once they're switched in.
        layout.add (createFilterParameters ("highPass", SynthProcessor::highPassIds, "High Pass", 250.0f));
        layout.add (createFilterParameters ("lowPass", SynthProcessor::lowPassIds, "Low Pass", 1000.0f));

        // The LFOs start on, as they do nothing until a control is assigned to one.
        const juce::NormalisableRange<float> periodRange (modulation::shortestPeriod, modulation::longestPeriod,
                                                          [] (float, float, float position) { return modulation::positionToPeriod (position); },
                                                          [] (float, float, float seconds) { return modulation::periodToPosition (seconds); });

        for (int i = 0; i < SynthProcessor::numLfos; ++i)
        {
            const auto name = "LFO " + juce::String (i + 1);

            layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
                "lfo" + juce::String (i + 1), name, " ",
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { SynthProcessor::lfoOnId (i), 1 }, name + " On", true),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::lfoPeriodId (i), 1 }, name + " Period", periodRange,
                    startingPeriods[(size_t) i],
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float value, int) { return formatPeriod (value); })
                        .withValueFromStringFunction ([] (const juce::String& text)
                        {
                            return text.retainCharacters ("0123456789.").getFloatValue();
                        })),
                std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { SynthProcessor::lfoModeId (i), 1 }, name + " Mode",
                    juce::StringArray { "Loop", "Once" }, 0)));
        }

        for (int i = 0; i < SynthProcessor::numEnvelopes; ++i)
        {
            const auto name = "Env " + juce::String (i + 1);
            const auto& starting = startingEnvelopes[(size_t) i];

            layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
                "env" + juce::String (i + 1), name, " ",
                createTimeParameter (SynthProcessor::attackId (i), name + " Attack", starting.attack),
                createTimeParameter (SynthProcessor::decayId (i), name + " Decay", starting.decay),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::sustainId (i), 1 }, name + " Sustain",
                    juce::NormalisableRange<float> (0.0f, 1.0f), starting.sustain, asPercentage()),
                createTimeParameter (SynthProcessor::releaseId (i), name + " Release", starting.release)));
        }

        return layout;
    }

    /** Presets are listed in order of name, with numbers counted as numbers. */
    bool comesBefore (const juce::String& name, const juce::String& other)
    {
        return name.compareNatural (other) < 0;
    }

    /** A factory preset: its name, and its XML, which is built into EZ Synth. */
    struct FactoryPreset
    {
        juce::String name;
        juce::String xml;
    };

    std::vector<FactoryPreset> getFactoryPresets()
    {
        std::vector<FactoryPreset> presets;
        const juce::String extension (SynthProcessor::presetExtension);

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String filename (BinaryData::originalFilenames[i]);
            auto size = 0;

            if (const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
                data != nullptr && filename.endsWith (extension))
                presets.push_back ({ filename.dropLastCharacters (extension.length()), juce::String::fromUTF8 (data, size) });
        }

        std::sort (presets.begin(), presets.end(), [] (const FactoryPreset& a, const FactoryPreset& b)
        {
            return comesBefore (a.name, b.name);
        });

        return presets;
    }
}

juce::String SynthProcessor::onId (int oscillator)          { return modulation::getKey (modulation::oscillatorOnTarget (oscillator)); }
juce::String SynthProcessor::levelId (int oscillator)       { return modulation::getKey (modulation::oscillatorLevelTarget (oscillator)); }
juce::String SynthProcessor::harmonicsId (int oscillator)   { return modulation::getKey (modulation::oscillatorHarmonicsTarget (oscillator)); }
juce::String SynthProcessor::tuneId (int oscillator)        { return modulation::getKey (modulation::oscillatorTuneTarget (oscillator)); }
juce::String SynthProcessor::lfoOnId (int lfo)              { return "lfo" + juce::String (lfo + 1) + "On"; }
juce::String SynthProcessor::lfoPeriodId (int lfo)          { return "lfo" + juce::String (lfo + 1) + "Period"; }
juce::String SynthProcessor::lfoModeId (int lfo)            { return "lfo" + juce::String (lfo + 1) + "Mode"; }
juce::String SynthProcessor::attackId (int envelope)        { return "env" + juce::String (envelope + 1) + "Attack"; }
juce::String SynthProcessor::decayId (int envelope)         { return "env" + juce::String (envelope + 1) + "Decay"; }
juce::String SynthProcessor::sustainId (int envelope)       { return "env" + juce::String (envelope + 1) + "Sustain"; }
juce::String SynthProcessor::releaseId (int envelope)       { return "env" + juce::String (envelope + 1) + "Release"; }

SynthProcessor::SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "EZSynth", createParameters())
{
    volume = parameters.getRawParameterValue (volumeId);

    for (int i = 0; i < numOscillators; ++i)
        oscillatorParameters[(size_t) i] = { parameters.getRawParameterValue (onId (i)),
                                             parameters.getRawParameterValue (levelId (i)),
                                             parameters.getRawParameterValue (harmonicsId (i)),
                                             parameters.getRawParameterValue (tuneId (i)) };

    for (auto [filter, ids] : { std::pair (modulation::lowPass, lowPassIds), std::pair (modulation::highPass, highPassIds) })
        filterParameters[(size_t) filter] = { parameters.getRawParameterValue (ids.on),
                                              parameters.getRawParameterValue (ids.cutoff),
                                              parameters.getRawParameterValue (ids.resonance) };

    for (int i = 0; i < numLfos; ++i)
        lfoParameters[(size_t) i] = { parameters.getRawParameterValue (lfoOnId (i)),
                                      parameters.getRawParameterValue (lfoPeriodId (i)),
                                      parameters.getRawParameterValue (lfoModeId (i)) };

    for (int i = 0; i < numEnvelopes; ++i)
        envelopeParameters[(size_t) i] = { parameters.getRawParameterValue (attackId (i)),
                                           parameters.getRawParameterValue (decayId (i)),
                                           parameters.getRawParameterValue (sustainId (i)),
                                           parameters.getRawParameterValue (releaseId (i)) };

    for (auto& progress : lfoProgress)
        progress = -1.0;

    for (int i = 0; i < numVoices; ++i)
        voices.push_back (dynamic_cast<HarmonicVoice*> (synth.addVoice (new HarmonicVoice (patch))));

    synth.addSound (new HarmonicSound());

    velocityAssignments[(size_t) modulation::volumeTarget] = modulation::startingVelocityOnVolume;

    for (int i = 0; i < numOscillators; ++i)
        setShape (i, waveform::makePreset (startingPresets[(size_t) i]), startingPresets[(size_t) i]);

    for (int i = 0; i < numLfos; ++i)
        setLfoShape (i, waveform::makePreset (startingLfoPresets[(size_t) i]), startingLfoPresets[(size_t) i]);

    // Any change to a parameter, from the window or a host, means the sound is no longer the preset's.
    for (auto* parameter : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            parameters.addParameterListener (withId->paramID, this);

    // Init is the sound as it starts, before anything's been edited.
    presetEdited = false;
    initState = createState();
}

SynthProcessor::~SynthProcessor()
{
    for (auto* parameter : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            parameters.removeParameterListener (withId->paramID, this);
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
    playingSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    // The looping LFOs start their periods afresh whenever playing starts.
    lfoPhases.fill (0.0);
}

void SynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();

    // MIDI that's played, rather than a device's clock or the like, counts as arriving.
    if (std::any_of (midi.begin(), midi.end(), [] (const juce::MidiMessageMetadata& metadata)
                     {
                         const auto message = metadata.getMessage();
                         return message.isNoteOnOrOff() || message.isController() || message.isPitchWheel()
                             || message.isAftertouch() || message.isChannelPressure() || message.isProgramChange();
                     }))
        ++midiReceived;

    // Adds the notes played on the on-screen keyboard, and lights its keys for notes played from elsewhere.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    for (const auto metadata : midi)
    {
        if (const auto message = metadata.getMessage(); message.isNoteOn())
        {
            lastNote = message.getNoteNumber();
            lastNoteOnSample = samplesPlayed + metadata.samplePosition;
        }
    }

    updatePatch();
    synth.renderNextBlock (buffer, midi, 0, numSamples);
    shareNewestNote();

    for (size_t i = 0; i < lfoPhases.size(); ++i)
    {
        const auto& lfo = patch.lfos[i];
        lfoPhases[i] += numSamples / lfo.periodSamples;
        lfoPhases[i] -= std::floor (lfoPhases[i]);

        if (! lfo.once)
            lfoProgress[i] = lfoPhases[i];
        else if (lastNoteOnSample >= 0)
            lfoProgress[i] = (double) (samplesPlayed + numSamples - lastNoteOnSample) / lfo.periodSamples;
        else
            lfoProgress[i] = -1.0;
    }

    samplesPlayed += numSamples;
}

void SynthProcessor::shareNewestNote()
{
    const HarmonicVoice* newest = nullptr;

    for (const auto* voice : voices)
        if (voice->isVoiceActive() && (newest == nullptr || voice->getSamplesSinceNoteOn() < newest->getSamplesSinceNoteOn()))
            newest = voice;

    noteSounding = newest != nullptr;

    if (newest == nullptr)
        return;

    newestVelocity = newest->getVelocity();

    for (int i = 0; i < numEnvelopes; ++i)
    {
        const auto& run = newest->getEnvelopeRun (i);
        auto& display = envelopeDisplays[(size_t) i];
        display.stage = (int) run.getStage();
        display.progress = run.getProgress (patch.envelopes[(size_t) i]);
        display.level = run.getLevel();
    }
}

void SynthProcessor::updatePatch()
{
    if (sharedVersion.load() != receivedVersion)
    {
        auto taken = false;

        {
            const juce::SpinLock::ScopedTryLockType lock (sharedLock);

            if (lock.isLocked())
            {
                received = shared;
                receivedVersion = sharedVersion.load();
                taken = true;
            }
        }

        if (taken)
        {
            for (size_t i = 0; i < (size_t) numOscillators; ++i)
            {
                auto& oscillator = patch.oscillators[i];
                const auto& decomposition = received.decompositions[i];

                for (size_t k = 0; k < (size_t) waveform::maxHarmonics; ++k)
                {
                    // A harmonic the wave hasn't got, if an LFO raises it, comes in at the phase it last had.
                    const auto& harmonic = decomposition.harmonics[k];
                    const auto phase = harmonic.amplitude > 0.0f ? harmonic.phase : received.phases[i][k];
                    oscillator.amplitudes[k] = harmonic.amplitude;
                    oscillator.phaseSines[k] = std::sin (phase);
                    oscillator.phaseCosines[k] = std::cos (phase);
                }

                oscillator.peaks = decomposition.peaks;
            }

            for (size_t i = 0; i < (size_t) numLfos; ++i)
                patch.lfos[i].shape = received.lfoShapes[i];

            patch.assignments = received.assignments;
            patch.velocityAssignments = received.velocityAssignments;
        }
    }

    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        const auto& source = oscillatorParameters[i];
        auto& oscillator = patch.oscillators[i];
        oscillator.on = source.on->load() >= 0.5f;
        oscillator.level = source.level->load();
        oscillator.numHarmonics = juce::jlimit (1, waveform::maxHarmonics, juce::roundToInt (source.numHarmonics->load()));
        oscillator.tune = source.tune->load();
    }

    for (size_t i = 0; i < (size_t) modulation::numFilters; ++i)
        patch.filters[i] = { filterParameters[i].on->load() >= 0.5f, filterParameters[i].cutoff->load(),
                             filterParameters[i].resonance->load() };

    patch.volume = volume->load();

    for (size_t i = 0; i < (size_t) numEnvelopes; ++i)
        patch.envelopes[i] = { envelopeParameters[i].attack->load(), envelopeParameters[i].decay->load(),
                               envelopeParameters[i].sustain->load(), envelopeParameters[i].release->load() };

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        auto& lfo = patch.lfos[i];
        lfo.on = lfoParameters[i].on->load() >= 0.5f;
        lfo.once = lfoParameters[i].mode->load() >= 0.5f;
        lfo.periodSamples = std::max (1.0, lfoParameters[i].period->load() * playingSampleRate);
        lfo.phase = lfoPhases[i];
    }
}

//==============================================================================
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

int SynthProcessor::getNumHarmonics (int oscillator) const noexcept
{
    return juce::roundToInt (oscillatorParameters[(size_t) oscillator].numHarmonics->load());
}

float SynthProcessor::getTune (int oscillator) const noexcept
{
    return oscillatorParameters[(size_t) oscillator].tune->load();
}

void SynthProcessor::setShape (int oscillator, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    presetEdited = true;

    // A harmonic the new wave doesn't have will start like a sine, rising from zero, if it's raised.
    oscillators[(size_t) oscillator].phases.fill (0.0f);
    replaceShape (oscillator, newShape, fromPreset);
}

void SynthProcessor::replaceShape (int oscillator, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    auto& changed = oscillators[(size_t) oscillator];
    changed.shape = newShape;
    changed.preset = fromPreset;
    changed.decomposition = waveform::decompose (newShape);

    for (const auto& harmonic : changed.decomposition.harmonics)
        if (harmonic.amplitude > 0.0f)
            changed.phases[(size_t) harmonic.harmonic - 1] = harmonic.phase;

    share();
    ++shapeVersion;
}

void SynthProcessor::setHarmonicAmplitude (int oscillator, int harmonic, float amplitude)
{
    presetEdited = true;

    const auto& changed = oscillators[(size_t) oscillator];
    const auto& current = changed.decomposition.harmonics[(size_t) harmonic - 1];
    auto shape = changed.shape;

    // A harmonic being raised from nothing comes back in the phase it last had.
    if (current.amplitude > 0.0f)
        waveform::addHarmonic (shape, harmonic, amplitude - current.amplitude, current.phase);
    else
        waveform::addHarmonic (shape, harmonic, amplitude, changed.phases[(size_t) harmonic - 1]);

    replaceShape (oscillator, shape, std::nullopt);
}

//==============================================================================
const waveform::Shape& SynthProcessor::getLfoShape (int lfo) const noexcept
{
    return lfos[(size_t) lfo].shape;
}

std::optional<waveform::Preset> SynthProcessor::getLfoPreset (int lfo) const noexcept
{
    return lfos[(size_t) lfo].preset;
}

void SynthProcessor::setLfoShape (int lfo, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    presetEdited = true;

    lfos[(size_t) lfo] = { newShape, fromPreset };
    share();
    ++shapeVersion;
}

bool SynthProcessor::isLfoOn (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].on->load() >= 0.5f;
}

float SynthProcessor::getLfoPeriod (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].period->load();
}

bool SynthProcessor::isLfoOnce (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].mode->load() >= 0.5f;
}

std::optional<double> SynthProcessor::getLfoProgress (int lfo) const noexcept
{
    const auto progress = lfoProgress[(size_t) lfo].load();
    return progress >= 0.0 ? std::optional (progress) : std::nullopt;
}

float SynthProcessor::getLfoValue (int lfo, float phase) const
{
    return modulation::lfoValue (getLfoShape (lfo), getLfoProgress (lfo).value_or (0.0) - phase, isLfoOnce (lfo));
}

//==============================================================================
EnvelopeSettings SynthProcessor::getEnvelopeSettings (int envelope) const noexcept
{
    const auto& source = envelopeParameters[(size_t) envelope];
    return { source.attack->load(), source.decay->load(), source.sustain->load(), source.release->load() };
}

std::optional<SynthProcessor::EnvelopeDisplay> SynthProcessor::getEnvelopeDisplay (int envelope) const noexcept
{
    if (! noteSounding)
        return std::nullopt;

    const auto& display = envelopeDisplays[(size_t) envelope];
    return EnvelopeDisplay { (EnvelopeRun::Stage) display.stage.load(), display.progress.load(), display.level.load() };
}

double SynthProcessor::getTailLengthSeconds() const
{
    // A note fades for as long as the longest release of any envelope moving how loud it is.
    auto tail = (double) HarmonicVoice::releaseSeconds;

    for (int target = 0; target < modulation::numTargets; ++target)
        if (const auto& assignment = assignments[(size_t) target];
            assignment.isActive() && modulation::isEnvelope (assignment.source) && modulation::isLoudness (target))
            tail = std::max (tail, (double) getEnvelopeSettings (assignment.source - numLfos).release);

    return tail;
}

//==============================================================================
const modulation::Assignment& SynthProcessor::getAssignment (int target) const noexcept
{
    return assignments[(size_t) target];
}

void SynthProcessor::setAssignment (int target, const modulation::Assignment& assignment)
{
    presetEdited = true;

    assignments[(size_t) target] = assignment;
    assignments[(size_t) target].phase = juce::jlimit (0.0f, modulation::latestPhase, assignment.phase);

    // An LFO that's been given something to move is meant to be heard.
    if (assignment.isActive() && ! modulation::isEnvelope (assignment.source) && ! isLfoOn (assignment.source))
    {
        auto& lfoOn = *parameters.getParameter (lfoOnId (assignment.source));
        lfoOn.setValueNotifyingHost (1.0f);
    }

    share();
    ++modulationVersion;
}

const modulation::Assignment& SynthProcessor::getVelocityAssignment (int target) const noexcept
{
    return velocityAssignments[(size_t) target];
}

void SynthProcessor::setVelocityAssignment (int target, const modulation::Assignment& assignment)
{
    presetEdited = true;

    velocityAssignments[(size_t) target] = { assignment.source, assignment.range, false };
    share();
    ++modulationVersion;
}

std::optional<float> SynthProcessor::getSourceValue (int source, float phase) const
{
    if (source == modulation::velocitySource)
        return noteSounding ? std::optional (newestVelocity.load()) : std::nullopt;

    if (modulation::isEnvelope (source))
    {
        if (const auto display = getEnvelopeDisplay (source - numLfos))
            return display->level;

        return std::nullopt;
    }

    if (isLfoOn (source) && getLfoProgress (source).has_value())
        return getLfoValue (source, phase);

    return std::nullopt;
}

std::optional<float> SynthProcessor::getLivePosition (int target) const
{
    // The LFO or envelope and the velocity moving the control add up.
    std::optional<float> offset;

    for (const auto* assignment : { &assignments[(size_t) target], &velocityAssignments[(size_t) target] })
        if (assignment->isActive())
            if (const auto sourceValue = getSourceValue (assignment->source, assignment->phase))
                offset = offset.value_or (0.0f) + assignment->offset (*sourceValue);

    if (! offset.has_value())
        return std::nullopt;

    return juce::jlimit (0.0f, 1.0f, getPosition (target) + *offset);
}

bool SynthProcessor::LiveWave::operator== (const LiveWave& other) const noexcept
{
    return moved == other.moved
        && std::equal (harmonics.begin(), harmonics.end(), other.harmonics.begin(), other.harmonics.end(),
                       [] (const waveform::Partial& a, const waveform::Partial& b)
                       {
                           return a.harmonic == b.harmonic && juce::exactlyEqual (a.amplitude, b.amplitude)
                               && juce::exactlyEqual (a.phase, b.phase);
                       });
}

SynthProcessor::LiveWave SynthProcessor::getLiveWave (int oscillator) const
{
    // Moved as the voices move them (HarmonicVoice::getTargets).
    LiveWave wave;
    auto numHarmonics = getNumHarmonics (oscillator);

    if (const auto moved = getLivePosition (modulation::oscillatorHarmonicsTarget (oscillator)))
    {
        numHarmonics = modulation::positionToHarmonics (*moved);
        wave.moved = true;
    }

    const auto& harmonics = getDecomposition (oscillator).harmonics;

    for (int harmonic = 1; harmonic <= numHarmonics; ++harmonic)
    {
        auto partial = harmonics[(size_t) harmonic - 1];

        if (const auto moved = getLivePosition (modulation::harmonicTarget (oscillator, harmonic)))
        {
            partial.amplitude = modulation::amplitudeRange().convertFrom0to1 (*moved);
            wave.moved = true;
        }

        wave.harmonics.push_back (partial);
    }

    return wave;
}

float SynthProcessor::getPosition (int target) const
{
    if (modulation::isParameter (target))
        return parameters.getParameter (modulation::getKey (target))->getValue();

    const auto info = modulation::describe (target);
    const auto& harmonic = getDecomposition (info.oscillator).harmonics[(size_t) info.harmonic - 1];
    return modulation::amplitudeRange().convertTo0to1 (harmonic.amplitude);
}

void SynthProcessor::share()
{
    const juce::SpinLock::ScopedLockType lock (sharedLock);

    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        shared.decompositions[i] = oscillators[i].decomposition;
        shared.phases[i] = oscillators[i].phases;
    }

    for (size_t i = 0; i < (size_t) numLfos; ++i)
        shared.lfoShapes[i] = lfos[i].shape;

    shared.assignments = assignments;
    shared.velocityAssignments = velocityAssignments;
    ++sharedVersion;
}

//==============================================================================
void SynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (parameters.state.getType()))
        loadState (juce::ValueTree::fromXml (*xml));
}

juce::ValueTree SynthProcessor::createState()
{
    auto state = parameters.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetEdited", presetEdited.load(), nullptr);
    state.setProperty ("presetFactory", factoryPreset, nullptr);

    const auto addWave = [&state] (const juce::Identifier& source, int index, const waveform::Shape& shape,
                                   std::optional<waveform::Preset> preset)
    {
        juce::ValueTree wave ("WAVE");
        wave.setProperty (source, index, nullptr);
        wave.setProperty ("points", juce::MemoryBlock (shape.data(), sizeof (shape)).toBase64Encoding(), nullptr);
        wave.setProperty ("preset", preset.has_value() ? waveform::getName (*preset) : juce::String(), nullptr);
        state.appendChild (wave, nullptr);
    };

    for (int i = 0; i < numOscillators; ++i)
        addWave ("oscillator", i, oscillators[(size_t) i].shape, oscillators[(size_t) i].preset);

    for (int i = 0; i < numLfos; ++i)
        addWave ("lfo", i, lfos[(size_t) i].shape, lfos[(size_t) i].preset);

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto& assignment = assignments[(size_t) target]; assignment.isActive())
        {
            juce::ValueTree modulationTree ("MODULATION");
            modulationTree.setProperty ("target", modulation::getKey (target), nullptr);
            modulationTree.setProperty ("source", modulation::getSourceName (assignment.source), nullptr);
            modulationTree.setProperty ("range", assignment.range, nullptr);
            modulationTree.setProperty ("inverted", assignment.inverted, nullptr);

            if (! modulation::isEnvelope (assignment.source) && assignment.phase > 0.0f)
                modulationTree.setProperty ("phase", assignment.phase, nullptr);

            state.appendChild (modulationTree, nullptr);
        }
    }

    // Saved even when empty, so a patch whose velocity was taken off everything loads that way.
    juce::ValueTree velocityTree ("VELOCITY");

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto& assignment = velocityAssignments[(size_t) target]; assignment.isActive())
        {
            juce::ValueTree assignmentTree ("ASSIGNMENT");
            assignmentTree.setProperty ("target", modulation::getKey (target), nullptr);
            assignmentTree.setProperty ("range", assignment.range, nullptr);
            velocityTree.appendChild (assignmentTree, nullptr);
        }
    }

    state.appendChild (velocityTree, nullptr);
    return state;
}

void SynthProcessor::loadState (juce::ValueTree state)
{
    const auto name = state.getProperty ("presetName", initPresetName).toString();
    const auto edited = (bool) state.getProperty ("presetEdited", false);
    const auto factory = (bool) state.getProperty ("presetFactory", false);
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetEdited", nullptr);
    state.removeProperty ("presetFactory", nullptr);

    std::vector<juce::ValueTree> waves, modulations;

    for (auto child = state.getChildWithName ("WAVE"); child.isValid(); child = state.getChildWithName ("WAVE"))
    {
        waves.push_back (child);
        state.removeChild (child, nullptr);
    }

    for (auto child = state.getChildWithName ("MODULATION"); child.isValid(); child = state.getChildWithName ("MODULATION"))
    {
        modulations.push_back (child);
        state.removeChild (child, nullptr);
    }

    const auto velocityTree = state.getChildWithName ("VELOCITY");
    state.removeChild (velocityTree, nullptr);

    // A save from before a control existed, such as an oscillator's tune, gives it its default, rather than leaving
    // it as it was.
    for (auto* parameter : getParameters())
    {
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            withId != nullptr && ! state.getChildWithProperty ("id", withId->paramID).isValid())
        {
            juce::ValueTree missing ("PARAM");
            missing.setProperty ("id", withId->paramID, nullptr);
            missing.setProperty ("value", withId->convertFrom0to1 (withId->getDefaultValue()), nullptr);
            state.appendChild (missing, nullptr);
        }
    }

    parameters.replaceState (state);

    for (const auto& wave : waves)
    {
        juce::MemoryBlock points;

        if (! points.fromBase64Encoding (wave["points"].toString()) || points.getSize() != sizeof (waveform::Shape))
            continue;

        waveform::Shape restored {};
        std::memcpy (restored.data(), points.getData(), sizeof (restored));

        for (auto& value : restored)
            value = std::isfinite (value) ? juce::jlimit (-maxValue, maxValue, value) : 0.0f;

        const auto preset = waveform::findPreset (wave["preset"].toString());

        if (wave.hasProperty ("lfo"))
        {
            if (const auto lfo = (int) wave["lfo"]; lfo >= 0 && lfo < numLfos)
            {
                // Drawing keeps an LFO's wave between -1 and 1, as that's as far as it moves a control.
                for (auto& value : restored)
                    value = juce::jlimit (-1.0f, 1.0f, value);

                setLfoShape (lfo, restored, preset);
            }
        }
        else if (const auto oscillator = (int) wave.getProperty ("oscillator", 0); oscillator >= 0 && oscillator < numOscillators)
        {
            // Saves from before there were three oscillators have one wave, with no number.
            setShape (oscillator, restored, preset);
        }
    }

    assignments = {};

    for (const auto& modulationTree : modulations)
    {
        const auto target = modulation::findTarget (modulationTree["target"].toString());

        // Saves from before there were envelopes number the LFO instead of naming the source.
        auto source = modulationTree.hasProperty ("lfo") ? (int) modulationTree["lfo"] : -1;

        for (int i = 0; i < modulation::numSources; ++i)
            if (modulationTree["source"].toString() == modulation::getSourceName (i))
                source = i;

        if (target.has_value() && source >= 0 && source < modulation::numSources)
            assignments[(size_t) *target] = { source, juce::jlimit (0.0f, 1.0f, (float) modulationTree["range"]),
                                               (bool) modulationTree["inverted"],
                                               modulation::isEnvelope (source)
                                                   ? 0.0f
                                                   : juce::jlimit (0.0f, modulation::latestPhase, (float) modulationTree["phase"]) };
    }

    // Saves from before velocity could be assigned had it make softer notes quieter, as it does to start with.
    velocityAssignments = {};

    if (! velocityTree.isValid())
        velocityAssignments[(size_t) modulation::volumeTarget] = modulation::startingVelocityOnVolume;

    for (const auto& assignmentTree : velocityTree)
        if (const auto target = modulation::findTarget (assignmentTree["target"].toString()))
            velocityAssignments[(size_t) *target] = { modulation::velocitySource,
                                                      juce::jlimit (0.0f, 1.0f, (float) assignmentTree["range"]), false };

    share();
    ++modulationVersion;

    // Loading changed everything, which on its own would count as editing the preset.
    presetName = name;
    presetEdited = edited;
    factoryPreset = factory;
    ++presetVersion;
}

//==============================================================================
juce::File SynthProcessor::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EZ Synth").getChildFile ("Presets");
}

juce::Array<juce::File> SynthProcessor::findPresets (const juce::File& folder)
{
    auto presets = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension);

    std::sort (presets.begin(), presets.end(), [] (const juce::File& a, const juce::File& b)
    {
        return comesBefore (a.getFileNameWithoutExtension(), b.getFileNameWithoutExtension());
    });

    return presets;
}

bool SynthProcessor::savePreset (const juce::File& file)
{
    const auto named = file.withFileExtension (presetExtension);

    // The preset's name is its file's, so a preset renamed in Finder takes on the new name.
    auto state = createState();
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetEdited", nullptr);
    state.removeProperty ("presetFactory", nullptr);

    const auto xml = state.createXml();

    if (xml == nullptr || ! named.getParentDirectory().createDirectory() || ! xml->writeTo (named))
        return false;

    // Saved, a factory preset becomes one of the user's own.
    presetName = named.getFileNameWithoutExtension();
    presetFile = named;
    presetEdited = false;
    factoryPreset = false;
    ++presetVersion;
    return true;
}

bool SynthProcessor::loadPreset (const juce::File& file)
{
    if (! loadPresetXml (juce::XmlDocument::parse (file).get(), file.getFileNameWithoutExtension(), false))
        return false;

    presetFile = file;
    return true;
}

void SynthProcessor::loadInitPreset()
{
    loadState (initState);
    presetFile = juce::File();
}

juce::StringArray SynthProcessor::getFactoryPresetNames()
{
    juce::StringArray names;

    for (const auto& preset : getFactoryPresets())
        names.add (preset.name);

    return names;
}

bool SynthProcessor::loadFactoryPreset (const juce::String& name)
{
    for (const auto& preset : getFactoryPresets())
    {
        if (preset.name == name && loadPresetXml (juce::XmlDocument::parse (preset.xml).get(), name, true))
        {
            presetFile = juce::File();
            return true;
        }
    }

    return false;
}

bool SynthProcessor::loadPresetXml (const juce::XmlElement* xml, const juce::String& name, bool isFactory)
{
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return false;

    auto state = juce::ValueTree::fromXml (*xml);
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("presetEdited", false, nullptr);
    state.setProperty ("presetFactory", isFactory, nullptr);
    loadState (state);
    return true;
}

void SynthProcessor::parameterChanged (const juce::String&, float)
{
    presetEdited = true;
}

juce::AudioProcessorEditor* SynthProcessor::createEditor()
{
    return new SynthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthProcessor();
}
