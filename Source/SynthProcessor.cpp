#include "SynthProcessor.h"

#include <numeric>

#include "RandomWave.h"
#include "SynthEditor.h"

#include <BinaryData.h>

namespace
{
    constexpr int numVoices = 16;

    /** The waves the oscillators start with, so each sounds different when it's switched on: a sine, a square and
        a sawtooth, each with its first 3 harmonics.
    */
    constexpr std::array<waveform::Preset, SynthProcessor::numOscillators> startingPresets {
        waveform::Preset::sine, waveform::Preset::square, waveform::Preset::sawtooth
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

    /** An LFO's rate, from its period, such as "0.25 Hz" or "4.0 Hz". */
    juce::String formatRate (float seconds)
    {
        const auto hertz = 1.0f / seconds;
        return juce::String (hertz, hertz < 1.0f ? 2 : 1) + " Hz";
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

    std::unique_ptr<juce::AudioProcessorParameterGroup> createFilterParameters (int filter, FilterType defaultType,
                                                                                float defaultCutoff)
    {
        const auto ids = SynthProcessor::filterIds (filter);
        const auto name = "Filter " + juce::String (filter + 1);
        juce::StringArray typeNames;

        for (const auto type : allFilterTypes)
            typeNames.add (FilterStage::getName (type));

        const auto parseNumber = [] (const juce::String& text) { return text.retainCharacters ("0123456789.").getFloatValue(); };

        // Cutoffs are spread evenly by octave, as pitch is heard.
        const juce::NormalisableRange<float> cutoffRange (FilterStage::lowestCutoff, FilterStage::highestCutoff,
                                                          [] (float, float, float position) { return FilterStage::positionToCutoff (position); },
                                                          [] (float, float, float hertz) { return FilterStage::cutoffToPosition (hertz); });

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "filter" + juce::String (filter + 1), name, " ",
            std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids.on, 1 }, name + " On", false),
            std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids.type, 1 }, name + " Type", typeNames,
                                                          (int) defaultType),
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
                0.0f, asPercentage()),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.width, 1 }, name + " Width", FilterStage::widthRange(), 2.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float octaves, int) { return juce::String (octaves, 1) + " oct"; })
                    .withValueFromStringFunction (parseNumber)),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.q, 1 }, name + " Q", FilterStage::qRange(), 2.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float q, int) { return juce::String (q, q < 10.0f ? 1 : 0); })
                    .withValueFromStringFunction (parseNumber)),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.depth, 1 }, name + " Depth", FilterStage::depthRange(), 60.0f,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([] (float decibels, int) { return juce::String (juce::roundToInt (decibels)) + " dB"; })
                    .withValueFromStringFunction (parseNumber)));
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
                    juce::NormalisableRange<float> (tuning::lowest, tuning::highest), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float semitones, int) { return tuning::toText (semitones); })
                        .withValueFromStringFunction ([] (const juce::String& text)
                        {
                            const auto size = text.retainCharacters ("0123456789.").getFloatValue();
                            const auto negative = text.containsChar ('-') || text.contains (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")));
                            return negative ? -size : size;
                        })),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::tuneZoomId (i), 1 }, name + " Tune Zoom",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float zoom, int) { return tuning::reachToText (tuning::zoomToReach (zoom)); })),
                std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { SynthProcessor::glideId (i), 1 }, name + " Glide", SynthProcessor::getGlideNames(), 0),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::glideTimeId (i), 1 }, name + " Glide Time", SynthProcessor::glideTimeRange(), 0.2f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float seconds, int) { return SynthProcessor::glideTimeToText (seconds); }))));
        }

        // Both filters start switched out, a low pass and a high pass, at cutoffs that change the sound clearly once
        // they're switched in.
        layout.add (createFilterParameters (0, FilterType::lowPass, 1000.0f));
        layout.add (createFilterParameters (1, FilterType::highPass, 250.0f));

        // The LFOs start off, ready to switch on, which giving one something to move does. Each keeps its period, in seconds,
        // but it's shown as a rate, in hertz, which rises as its dial turns up, from 0.1 to 10 Hz.
        const juce::NormalisableRange<float> periodRange (modulation::shortestPeriod, modulation::longestPeriod,
                                                          [] (float, float, float position) { return modulation::positionToPeriod (1.0f - position); },
                                                          [] (float, float, float seconds) { return 1.0f - modulation::periodToPosition (seconds); });

        for (int i = 0; i < SynthProcessor::numLfos; ++i)
        {
            const auto name = "LFO " + juce::String (i + 1);

            layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
                "lfo" + juce::String (i + 1), name, " ",
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { SynthProcessor::lfoOnId (i), 1 }, name + " On", false),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { SynthProcessor::lfoPeriodId (i), 1 }, name + " Rate", periodRange,
                    startingPeriods[(size_t) i],
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float value, int) { return formatRate (value); })
                        .withValueFromStringFunction ([] (const juce::String& text)
                        {
                            const auto hertz = text.retainCharacters ("0123456789.").getFloatValue();
                            return hertz > 0.0f ? 1.0f / hertz : modulation::longestPeriod;
                        })),
                std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { SynthProcessor::lfoModeId (i), 1 }, name + " Mode",
                    modulation::getLfoModeNames(), modulation::lfoLoops),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { SynthProcessor::lfoRandomId (i), 1 }, name + " Random", false),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { SynthProcessor::lfoSyncId (i), 1 }, name + " Sync", false),
                std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { SynthProcessor::lfoBeatRateId (i), 1 },
                                                              name + " Beat Rate", modulation::getBeatRateNames(),
                                                              modulation::oneCycleABeat)));
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
juce::String SynthProcessor::tuneZoomId (int oscillator)    { return tuneId (oscillator) + "Zoom"; }
juce::String SynthProcessor::glideId (int oscillator)       { return "osc" + juce::String (oscillator + 1) + "Glide"; }
juce::String SynthProcessor::glideTimeId (int oscillator)   { return glideId (oscillator) + "Time"; }

juce::StringArray SynthProcessor::getGlideNames()   { return { "Off", "Glide", "Legato" }; }

juce::NormalisableRange<float> SynthProcessor::glideTimeRange()
{
    // From 5 ms to 5 seconds, each step along the slider lengthening the glide by the same factor.
    constexpr auto shortest = 0.005f, longest = 5.0f;
    return { shortest, longest,
             [] (float, float, float proportion) { return shortest * std::pow (longest / shortest, proportion); },
             [] (float, float, float seconds) { return std::log (seconds / shortest) / std::log (longest / shortest); } };
}

juce::String SynthProcessor::glideTimeToText (float seconds)
{
    return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" : juce::String (seconds, 2) + " s";
}

SynthProcessor::FilterIds SynthProcessor::filterIds (int filter)
{
    using namespace modulation;
    return { getKey (filterOnTarget (filter)), "filter" + juce::String (filter + 1) + "Type", getKey (filterCutoffTarget (filter)),
             getKey (filterResonanceTarget (filter)), getKey (filterWidthTarget (filter)), getKey (filterQTarget (filter)),
             getKey (filterDepthTarget (filter)) };
}
juce::String SynthProcessor::lfoOnId (int lfo)              { return "lfo" + juce::String (lfo + 1) + "On"; }
juce::String SynthProcessor::lfoPeriodId (int lfo)          { return "lfo" + juce::String (lfo + 1) + "Period"; }
juce::String SynthProcessor::lfoModeId (int lfo)            { return "lfo" + juce::String (lfo + 1) + "Mode"; }
juce::String SynthProcessor::lfoRandomId (int lfo)          { return "lfo" + juce::String (lfo + 1) + "Random"; }
juce::String SynthProcessor::lfoSyncId (int lfo)            { return "lfo" + juce::String (lfo + 1) + "Sync"; }
juce::String SynthProcessor::lfoBeatRateId (int lfo)        { return "lfo" + juce::String (lfo + 1) + "BeatRate"; }
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
                                             parameters.getRawParameterValue (tuneId (i)),
                                             parameters.getRawParameterValue (tuneZoomId (i)),
                                             parameters.getRawParameterValue (glideId (i)),
                                             parameters.getRawParameterValue (glideTimeId (i)) };

    for (int filter = 0; filter < modulation::numFilters; ++filter)
    {
        const auto ids = filterIds (filter);
        filterParameters[(size_t) filter] = { parameters.getRawParameterValue (ids.on),
                                              parameters.getRawParameterValue (ids.type),
                                              parameters.getRawParameterValue (ids.cutoff),
                                              parameters.getRawParameterValue (ids.resonance),
                                              parameters.getRawParameterValue (ids.width),
                                              parameters.getRawParameterValue (ids.q),
                                              parameters.getRawParameterValue (ids.depth) };
    }

    for (int i = 0; i < numLfos; ++i)
        lfoParameters[(size_t) i] = { parameters.getRawParameterValue (lfoOnId (i)),
                                      parameters.getRawParameterValue (lfoPeriodId (i)),
                                      parameters.getRawParameterValue (lfoModeId (i)),
                                      parameters.getRawParameterValue (lfoRandomId (i)),
                                      parameters.getRawParameterValue (lfoSyncId (i)),
                                      parameters.getRawParameterValue (lfoBeatRateId (i)) };

    // Each looping LFO's random wave is its own.
    for (auto& seed : randomSeeds)
        seed = (std::uint32_t) juce::Random::getSystemRandom().nextInt();

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

    // Init is the sound as it starts, before anything's been edited, and the first step of the undo history.
    presetEdited = false;
    initState = createState();
    undoHistory.push_back (initState);

    // The audio thread asks for oscillators' harmonics to be randomized; the message thread does it.
    startTimerHz (60);
}

SynthProcessor::~SynthProcessor()
{
    stopTimer();

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
    lfoCycles.fill (0);
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

    // The host's tempo, for LFOs synced to it, and while it plays, where it is in beats, from the start of its song.
    playingTempo = modulation::defaultTempo;
    playingBeat.reset();
    auto tempoFromHost = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
            {
                playingTempo = *bpm;
                tempoFromHost = true;
            }

            if (const auto beat = position->getPpqPosition(); position->getIsPlaying() && beat.hasValue())
                playingBeat = *beat;
        }
    }

    hostTempo = playingTempo;
    hostGivesTempo = tempoFromHost;

    updatePatch();

    // How far through its periods an LFO that starts with each key is, from the last key pressed.
    for (size_t i = 0; i < periodsSinceLastNote.size(); ++i)
    {
        if (lastNoteOnSample >= samplesPlayed)
            periodsSinceLastNote[i] = (double) (samplesPlayed + numSamples - lastNoteOnSample) / patch.getLfoPeriodSamples (i);
        else
            periodsSinceLastNote[i] += numSamples / patch.getLfoPeriodSamples (i);
    }

    synth.renderNextBlock (buffer, midi, 0, numSamples);
    modWheelShown = patch.modWheel;
    shareNewestNote();

    for (size_t i = 0; i < lfoPhases.size(); ++i)
    {
        // The mod wheel can change an LFO's rate, so it moves on by the periods it's been through at that rate.
        const auto& lfo = patch.lfos[i];
        const auto periodSamples = patch.getLfoPeriodSamples (i);
        lfoPhases[i] += numSamples / periodSamples;
        const auto periodsDone = std::floor (lfoPhases[i]);
        lfoPhases[i] -= periodsDone;
        lfoCycles[i] += (std::int64_t) periodsDone;
        lfoCyclesShown[i] = lfoCycles[i];

        // An LFO whose rate each note moves for itself is shown as the newest note plays it.
        if (patch.isLfoRateMovedByNote (i) && noteSounding)
        {
            const auto periods = newestLfoPeriods[i];
            lfoProgress[i] = lfo.fromKey ? periods : periods - std::floor (periods);

            if (! lfo.fromKey)
                lfoCyclesShown[i] = (std::int64_t) std::floor (periods);
        }
        else if (! lfo.fromKey)
        {
            lfoProgress[i] = lfoPhases[i];
        }
        else if (lastNoteOnSample >= 0)
        {
            lfoProgress[i] = periodsSinceLastNote[i];
        }
        else
        {
            lfoProgress[i] = -1.0;
        }
    }

    watchRandomizeTriggers (numSamples);

    samplesPlayed += numSamples;
}

void SynthProcessor::watchRandomizeTriggers (int numSamples)
{
    // An LFO is looked at every few samples through the block, so a quick one's crossings aren't missed, and one
    // that only touches a threshold near its end, at its lowest or highest, is caught there.
    constexpr auto lookInterval = HarmonicVoice::controlInterval;

    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        const auto target = (size_t) modulation::randomizeTarget ((int) i);
        auto& crossings = randomizeCrossings[i];
        auto triggered = false;

        const auto watch = [this, &triggered, numSamples] (modulation::ThresholdCrossing& crossing, const modulation::Assignment& assignment)
        {
            const auto threshold = modulation::getThresholdValue (assignment.source, assignment.threshold);
            const auto isLfo = assignment.isActive() && assignment.source < numLfos;
            const auto looks = isLfo ? std::max (1, (numSamples + lookInterval - 1) / lookInterval) : 1;

            for (int look = looks - 1; look >= 0; --look)
            {
                const auto periodsEarlier = isLfo ? (double) (look * lookInterval) / patch.getLfoPeriodSamples ((size_t) assignment.source) : 0.0;
                const auto value = assignment.isActive() ? getSourceValueNow (assignment.source, assignment.phase + periodsEarlier)
                                                         : std::nullopt;
                triggered = crossing.update (value, threshold, assignment.threshold) || triggered;
            }
        };

        watch (crossings[0], patch.assignments[target]);
        watch (crossings[1], patch.velocityAssignments[target]);
        watch (crossings[2], patch.modWheelAssignments[target]);

        if (triggered)
            ++randomizeRequests[i];
    }
}

std::optional<float> SynthProcessor::getSourceValueNow (int source, double phase) const
{
    // The last note's velocity stays until the next note, even once it's faded, so one note can be compared with the next.
    if (source == modulation::velocitySource)
        return lastNote >= 0 ? std::optional (newestVelocity.load()) : std::nullopt;

    if (source == modulation::modWheelSource)
        return patch.modWheel;

    // With no note sounding, an envelope's at rest, at 0, so a note's attack crosses halfway on the way up.
    if (modulation::isEnvelope (source))
        return noteSounding ? envelopeDisplays[(size_t) (source - numLfos)].level.load() : 0.0f;

    if (source < 0 || source >= numLfos || ! patch.lfos[(size_t) source].on)
        return std::nullopt;

    const auto& lfo = patch.lfos[(size_t) source];
    const auto progress = lfoProgress[(size_t) source].load();

    if (progress < 0.0)
        return std::nullopt;

    const auto periods = (lfo.fromKey ? progress : (double) lfoCyclesShown[(size_t) source].load() + progress) - phase;

    if (! lfo.random)
        return modulation::lfoValue (lfo.shape, periods, lfo.fromKey, lfo.times);

    return lfo.fromKey ? randomwave::fromKeyValueAt (newestOnceSeeds[(size_t) source].load(), periods, lfo.drawing, lfo.times)
                       : randomwave::valueAt (lfo.seed, periods, lfo.drawing);
}

void SynthProcessor::randomizeAsTriggered()
{
    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        const auto requests = randomizeRequests[i].load();

        if (requests != randomizeRequestsDone[i])
        {
            randomizeRequestsDone[i] = requests;
            randomizeHarmonics ((int) i, randomizer);
            ++randomizeCounts[i];
        }
    }
}

void SynthProcessor::timerCallback()
{
    randomizeAsTriggered();
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

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        newestOnceSeeds[i] = newest->getRandomSeed ((int) i);
        newestLfoPeriods[i] = newest->getLfoPeriods ((int) i);
    }

    for (int i = 0; i < numEnvelopes; ++i)
    {
        const auto& run = newest->getEnvelopeRun (i);
        auto& display = envelopeDisplays[(size_t) i];
        display.stage = (int) run.getStage();
        display.progress = run.getProgress (patch.envelopes[(size_t) i]);
        display.level = run.getLevel();
        display.releaseFrom = run.getReleaseFrom();
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
                    oscillator.phases[k] = phase;
                    oscillator.phaseSines[k] = std::sin (phase);
                    oscillator.phaseCosines[k] = std::cos (phase);
                }

                oscillator.peaks = decomposition.peaks;
                oscillator.tunes = received.tunes[i];
            }

            for (size_t i = 0; i < (size_t) numLfos; ++i)
            {
                patch.lfos[i].shape = received.lfoShapes[i];
                patch.lfos[i].drawing = received.lfoDrawings[i];
            }

            patch.assignments = received.assignments;
            patch.velocityAssignments = received.velocityAssignments;
            patch.modWheelAssignments = received.modWheelAssignments;
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
        oscillator.tuneReach = tuning::zoomToReach (source.tuneZoom->load());
        oscillator.glide = (Patch::Glide) juce::jlimit (0, 2, juce::roundToInt (source.glide->load()));
        oscillator.glideSeconds = source.glideTime->load();
    }

    for (size_t i = 0; i < (size_t) modulation::numFilters; ++i)
        patch.filters[i] = { filterParameters[i].on->load() >= 0.5f,
                             (FilterType) juce::jlimit (0, (int) allFilterTypes.size() - 1, juce::roundToInt (filterParameters[i].type->load())),
                             filterParameters[i].cutoff->load(), filterParameters[i].resonance->load(),
                             filterParameters[i].width->load(), filterParameters[i].q->load(), filterParameters[i].depth->load() };

    patch.volume = volume->load();

    for (size_t i = 0; i < (size_t) numEnvelopes; ++i)
        patch.envelopes[i] = { envelopeParameters[i].attack->load(), envelopeParameters[i].decay->load(),
                               envelopeParameters[i].sustain->load(), envelopeParameters[i].release->load() };

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        auto& lfo = patch.lfos[i];
        lfo.on = lfoParameters[i].on->load() >= 0.5f;
        const auto mode = getLfoMode ((int) i);
        lfo.fromKey = mode != modulation::lfoLoops;
        lfo.times = modulation::getLfoTimes (mode);
        lfo.synced = isLfoSynced ((int) i);

        // Synced to the tempo, it runs a number of cycles a beat, and while the host plays, a looping LFO follows its
        // beat, so its periods line up with the beats, as a bar's do.
        if (lfo.synced)
        {
            const auto beats = getLfoSyncedBeats ((int) i);
            lfo.periodSeconds = (float) (beats * 60.0 / playingTempo);

            if (playingBeat.has_value())
            {
                const auto periods = *playingBeat / beats;
                lfoCycles[i] = (std::int64_t) std::floor (periods);
                lfoPhases[i] = periods - std::floor (periods);
            }
        }
        else
        {
            lfo.periodSeconds = lfoParameters[i].period->load();
        }

        lfo.periodSamples = std::max (1.0, lfo.periodSeconds * playingSampleRate);
        lfo.phase = lfoPhases[i];
        lfo.cycle = lfoCycles[i];
        lfo.random = lfoParameters[i].random->load() >= 0.5f;
        lfo.seed = randomSeeds[i];
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
    markEdited();

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
    markEdited();

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

float SynthProcessor::getHarmonicTune (int oscillator, int harmonic) const noexcept
{
    return oscillators[(size_t) oscillator].tunes[(size_t) harmonic - 1];
}

void SynthProcessor::setHarmonicTune (int oscillator, int harmonic, float tune)
{
    if (harmonic < 2 || harmonic > waveform::maxHarmonics)
        return;

    markEdited();
    oscillators[(size_t) oscillator].tunes[(size_t) harmonic - 1] = juce::jlimit (-waveform::furthestTune, waveform::furthestTune, tune);
    share();
    ++shapeVersion;
}

float SynthProcessor::getHarmonicPhase (int oscillator, int harmonic) const noexcept
{
    const auto& source = oscillators[(size_t) oscillator];
    const auto& current = source.decomposition.harmonics[(size_t) harmonic - 1];
    const auto phase = current.amplitude > 0.0f ? current.phase : source.phases[(size_t) harmonic - 1];
    const auto turn = juce::MathConstants<float>::twoPi;
    return phase - turn * std::floor (phase / turn);
}

void SynthProcessor::setHarmonicPhase (int oscillator, int harmonic, float phase)
{
    markEdited();

    auto& changed = oscillators[(size_t) oscillator];
    const auto current = changed.decomposition.harmonics[(size_t) harmonic - 1];
    changed.phases[(size_t) harmonic - 1] = phase;

    if (current.amplitude <= 0.0f)
    {
        share();
        ++shapeVersion;
        return;
    }

    // The harmonic's sine wave is taken out of the wave and put back at its new phase.
    auto shape = changed.shape;
    waveform::addHarmonic (shape, harmonic, -current.amplitude, current.phase);
    waveform::addHarmonic (shape, harmonic, current.amplitude, phase);
    replaceShape (oscillator, shape, std::nullopt);
}

void SynthProcessor::randomizeHarmonics (int oscillator, juce::Random& random)
{
    markEdited();

    std::array<float, waveform::maxHarmonics> amplitudes {}, phases {};

    for (size_t i = 0; i < amplitudes.size(); ++i)
    {
        amplitudes[i] = modulation::amplitudeRange().convertFrom0to1 (random.nextFloat());
        phases[i] = random.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    const auto build = [&amplitudes, &phases] (float scale)
    {
        waveform::Shape shape {};

        for (size_t i = 0; i < amplitudes.size(); ++i)
            waveform::addHarmonic (shape, (int) i + 1, amplitudes[i] * scale, phases[i]);

        return shape;
    };

    auto shape = build (1.0f);
    const auto peak = std::accumulate (shape.begin(), shape.end(), 0.0f, [] (float a, float b) { return std::max (a, std::abs (b)); });

    if (peak > 1.0f)
        shape = build (1.0f / peak);

    replaceShape (oscillator, shape, std::nullopt);

    // Harmonics too weak to keep come back in at their new phases when they're raised.
    std::copy (phases.begin(), phases.end(), oscillators[(size_t) oscillator].phases.begin());
    share();
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
    markEdited();

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
    if (isLfoSynced (lfo))
        return (float) (getLfoSyncedBeats (lfo) * 60.0 / getTempo());

    return lfoParameters[(size_t) lfo].period->load();
}

bool SynthProcessor::isLfoSynced (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].sync->load() >= 0.5f;
}

double SynthProcessor::getLfoCyclesPerBeat (int lfo) const noexcept
{
    const auto index = juce::roundToInt (lfoParameters[(size_t) lfo].beatRate->load());
    return modulation::beatRates[(size_t) juce::jlimit (0, (int) modulation::beatRates.size() - 1, index)];
}

int SynthProcessor::getLfoMode (int lfo) const noexcept
{
    return juce::jlimit (0, modulation::lfoForever, juce::roundToInt (lfoParameters[(size_t) lfo].mode->load()));
}

bool SynthProcessor::isLfoFromKey (int lfo) const noexcept
{
    return getLfoMode (lfo) != modulation::lfoLoops;
}

int SynthProcessor::getLfoTimes (int lfo) const noexcept
{
    return modulation::getLfoTimes (getLfoMode (lfo));
}

bool SynthProcessor::isLfoOnLastTime (int lfo) const noexcept
{
    const auto times = getLfoTimes (lfo);
    return isLfoFromKey (lfo) && times > 0 && getLfoProgress (lfo).value_or (0.0) >= (double) (times - 1);
}

std::optional<double> SynthProcessor::getLfoProgress (int lfo) const noexcept
{
    const auto progress = lfoProgress[(size_t) lfo].load();
    return progress >= 0.0 ? std::optional (progress) : std::nullopt;
}

float SynthProcessor::getLfoValue (int lfo, float phase) const
{
    const auto progress = getLfoProgress (lfo).value_or (0.0) - phase;

    if (! isLfoRandom (lfo))
        return modulation::lfoValue (getLfoShape (lfo), progress, isLfoFromKey (lfo), getLfoTimes (lfo));

    const auto random = getRandomCycle (lfo);
    const auto& drawing = getDrawing (numOscillators + lfo);

    if (isLfoFromKey (lfo))
        return randomwave::fromKeyValueAt (random.seed, progress, drawing, getLfoTimes (lfo));

    return randomwave::valueAt (random.seed, (double) random.cycle + progress, drawing);
}

bool SynthProcessor::isLfoRandom (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].random->load() >= 0.5f;
}

SynthProcessor::RandomCycle SynthProcessor::getRandomCycle (int lfo) const noexcept
{
    if (isLfoFromKey (lfo))
    {
        // The cycle the newest note is in, staying on the last once an LFO that plays a number of times has finished.
        const auto times = getLfoTimes (lfo);
        auto cycle = (std::int64_t) std::floor (std::max (0.0, getLfoProgress (lfo).value_or (0.0)));

        if (times > 0)
            cycle = std::min (cycle, (std::int64_t) times - 1);

        return { newestOnceSeeds[(size_t) lfo].load(), cycle };
    }

    return { randomSeeds[(size_t) lfo], lfoCyclesShown[(size_t) lfo].load() };
}

waveform::Shape SynthProcessor::getPlayedLfoShape (int lfo) const
{
    if (! isLfoRandom (lfo))
        return getLfoShape (lfo);

    const auto random = getRandomCycle (lfo);
    return randomwave::getCycle (random.seed, random.cycle, getDrawing (numOscillators + lfo));
}

std::optional<float> SynthProcessor::getPlayedLfoEnd (int lfo) const
{
    if (! isLfoRandom (lfo) || isLfoOnLastTime (lfo))
        return std::nullopt;

    const auto random = getRandomCycle (lfo);
    return randomwave::valueAt (random.seed, (double) random.cycle + 1.0, getDrawing (numOscillators + lfo));
}

void SynthProcessor::setDrawing (int source, const waveform::Drawing& drawing)
{
    drawings[(size_t) source] = drawing;
    ++changeCount;
    share();
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
    return EnvelopeDisplay { (EnvelopeRun::Stage) display.stage.load(), display.progress.load(), display.level.load(),
                             display.releaseFrom.load() };
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
    // An LFO can't move its own rate, even through other LFOs.
    if (wouldMakeLoop (target, assignment.source))
        return;

    markEdited();

    assignments[(size_t) target] = assignment;
    assignments[(size_t) target].phase = juce::jlimit (0.0f, modulation::latestPhase, assignment.phase);
    assignments[(size_t) target].threshold = juce::jlimit (0.0f, 1.0f, assignment.threshold);

    // An LFO that's been given something to move is meant to be heard.
    if (assignment.isActive() && ! modulation::isEnvelope (assignment.source) && ! isLfoOn (assignment.source))
    {
        auto& lfoOn = *parameters.getParameter (lfoOnId (assignment.source));
        lfoOn.setValueNotifyingHost (1.0f);
    }

    share();
    ++modulationVersion;
}

bool SynthProcessor::wouldMakeLoop (int target, int source) const
{
    return modulation::makesLoop (assignments, target, source);
}

const modulation::Assignment& SynthProcessor::getVelocityAssignment (int target) const noexcept
{
    return velocityAssignments[(size_t) target];
}

void SynthProcessor::setVelocityAssignment (int target, const modulation::Assignment& assignment)
{
    markEdited();

    velocityAssignments[(size_t) target] = { assignment.source, assignment.range, false, 0.0f, juce::jlimit (0.0f, 1.0f, assignment.threshold) };
    share();
    ++modulationVersion;
}

const modulation::Assignment& SynthProcessor::getModWheelAssignment (int target) const noexcept
{
    return modWheelAssignments[(size_t) target];
}

void SynthProcessor::setModWheelAssignment (int target, const modulation::Assignment& assignment)
{
    markEdited();

    modWheelAssignments[(size_t) target] = { assignment.source, assignment.range, assignment.inverted, 0.0f,
                                             juce::jlimit (0.0f, 1.0f, assignment.threshold) };
    share();
    ++modulationVersion;
}

std::optional<float> SynthProcessor::getSourceValue (int source, float phase) const
{
    if (source == modulation::velocitySource)
        return noteSounding ? std::optional (newestVelocity.load()) : std::nullopt;

    // The mod wheel is wherever it was last put, whether a note is sounding or not.
    if (source == modulation::modWheelSource)
        return modWheelShown.load();

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
    // A synced LFO's rate is the tempo's, which nothing moves, and randomizing is triggered, not moved.
    if (const auto info = modulation::describe (target); (info.kind == modulation::Kind::lfoRate && isLfoSynced (info.lfo))
                                                           || info.kind == modulation::Kind::randomize)
        return std::nullopt;

    // Moved as the voices move it (HarmonicVoice::getTargets): by velocity and the mod wheel, then by the LFO or envelope,
    // each across less of an oscillator's tune when its slider's zoomed in.
    auto position = getPosition (target);
    auto moved = false;
    const auto info = modulation::describe (target);
    const auto scale = info.kind == modulation::Kind::oscillatorTune
                           ? tuning::getModulationScale (tuning::zoomToReach (parameters.getRawParameterValue (tuneZoomId (info.oscillator))->load()))
                           : 1.0f;

    if (const auto& byVelocity = velocityAssignments[(size_t) target]; byVelocity.isActive())
    {
        if (const auto velocity = getSourceValue (modulation::velocitySource))
        {
            position += scale * byVelocity.offset (*velocity);
            moved = true;
        }
    }

    if (const auto& byModWheel = modWheelAssignments[(size_t) target]; byModWheel.isActive())
    {
        position += scale * byModWheel.offset (modWheelShown.load());
        moved = true;
    }

    if (const auto& assignment = assignments[(size_t) target]; assignment.isActive())
    {
        if (modulation::isEnvelope (assignment.source))
        {
            if (const auto envelope = getEnvelopeDisplay (assignment.source - numLfos))
                return modulation::applyEnvelope (target, assignment, position, envelope->getState(), scale);
        }
        else if (const auto lfo = getSourceValue (assignment.source, assignment.phase))
        {
            position += scale * assignment.offset (*lfo);
            moved = true;
        }
    }

    return moved ? std::optional (modulation::keepInTravel (target, position)) : std::nullopt;
}

bool SynthProcessor::LiveWave::operator== (const LiveWave& other) const noexcept
{
    return moved == other.moved
        && std::equal (harmonics.begin(), harmonics.end(), other.harmonics.begin(), other.harmonics.end(),
                       [] (const waveform::Partial& a, const waveform::Partial& b)
                       {
                           return a.harmonic == b.harmonic && juce::exactlyEqual (a.amplitude, b.amplitude)
                               && juce::exactlyEqual (a.phase, b.phase) && juce::exactlyEqual (a.tune, b.tune);
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
        partial.tune = getHarmonicTune (oscillator, harmonic);
        partial.phase = getHarmonicPhase (oscillator, harmonic);

        if (const auto moved = getLivePosition (modulation::harmonicPhaseTarget (oscillator, harmonic)))
        {
            partial.phase = *moved * juce::MathConstants<float>::twoPi;
            wave.moved = true;
        }

        if (const auto moved = getLivePosition (modulation::harmonicTarget (oscillator, harmonic)))
        {
            partial.amplitude = modulation::amplitudeRange().convertFrom0to1 (*moved);
            wave.moved = true;
        }

        if (harmonic > 1)
        {
            if (const auto moved = getLivePosition (modulation::harmonicTuneTarget (oscillator, harmonic)))
            {
                partial.tune = (*moved - 0.5f) * 2.0f * waveform::furthestTune;
                wave.moved = true;
            }
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

    // Randomizing isn't anywhere along a travel.
    if (info.kind == modulation::Kind::randomize)
        return 0.0f;

    // A harmonic's tune is in tune in the middle of its travel, and its phase goes from 0 round to a whole turn.
    if (info.kind == modulation::Kind::harmonicTune)
        return getHarmonicTune (info.oscillator, info.harmonic) / (2.0f * waveform::furthestTune) + 0.5f;

    if (info.kind == modulation::Kind::harmonicPhase)
        return getHarmonicPhase (info.oscillator, info.harmonic) / juce::MathConstants<float>::twoPi;

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
        shared.tunes[i] = oscillators[i].tunes;
    }

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        shared.lfoShapes[i] = lfos[i].shape;
        shared.lfoDrawings[i] = drawings[(size_t) numOscillators + i];
    }

    shared.assignments = assignments;
    shared.velocityAssignments = velocityAssignments;
    shared.modWheelAssignments = modWheelAssignments;
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
                                   std::optional<waveform::Preset> preset, const waveform::Drawing& drawing)
    {
        juce::ValueTree wave ("WAVE");
        wave.setProperty (source, index, nullptr);
        wave.setProperty ("points", juce::MemoryBlock (shape.data(), sizeof (shape)).toBase64Encoding(), nullptr);
        wave.setProperty ("preset", preset.has_value() ? waveform::getName (*preset) : juce::String(), nullptr);
        wave.setProperty ("stepped", drawing.stepped, nullptr);
        wave.setProperty ("steps", drawing.numSteps, nullptr);
        state.appendChild (wave, nullptr);
        return wave;
    };

    for (int i = 0; i < numOscillators; ++i)
    {
        auto wave = addWave ("oscillator", i, oscillators[(size_t) i].shape, oscillators[(size_t) i].preset, drawings[(size_t) i]);

        // How far each harmonic from the 2nd up is tuned, saved only if any is.
        const auto& tunes = oscillators[(size_t) i].tunes;

        if (std::any_of (tunes.begin(), tunes.end(), [] (float tune) { return ! juce::exactlyEqual (tune, 0.0f); }))
        {
            juce::StringArray tuneTexts;

            for (size_t k = 1; k < tunes.size(); ++k)
                tuneTexts.add (juce::String (tunes[k]));

            wave.setProperty ("tunes", tuneTexts.joinIntoString (" "), nullptr);
        }
    }

    for (int i = 0; i < numLfos; ++i)
        addWave ("lfo", i, lfos[(size_t) i].shape, lfos[(size_t) i].preset, drawings[(size_t) (numOscillators + i)]);

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

            if (modulation::isTrigger (target))
                modulationTree.setProperty ("threshold", assignment.threshold, nullptr);

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

            if (modulation::isTrigger (target))
                assignmentTree.setProperty ("threshold", assignment.threshold, nullptr);

            velocityTree.appendChild (assignmentTree, nullptr);
        }
    }

    state.appendChild (velocityTree, nullptr);

    juce::ValueTree modWheelTree ("MODWHEEL");

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto& assignment = modWheelAssignments[(size_t) target]; assignment.isActive())
        {
            juce::ValueTree assignmentTree ("ASSIGNMENT");
            assignmentTree.setProperty ("target", modulation::getKey (target), nullptr);
            assignmentTree.setProperty ("range", assignment.range, nullptr);
            assignmentTree.setProperty ("inverted", assignment.inverted, nullptr);

            if (modulation::isTrigger (target))
                assignmentTree.setProperty ("threshold", assignment.threshold, nullptr);

            modWheelTree.appendChild (assignmentTree, nullptr);
        }
    }

    state.appendChild (modWheelTree, nullptr);
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
    const auto modWheelTree = state.getChildWithName ("MODWHEEL");
    state.removeChild (modWheelTree, nullptr);

    // A save from before the filters could each be any type names the first filter's parameters "lowPass", and the
    // second's "highPass". The first was a low pass and the second a high pass, as the filters' types start.
    for (auto parameter : state)
        if (parameter.hasProperty ("id"))
            parameter.setProperty ("id", modulation::updateKey (parameter["id"].toString()), nullptr);

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

    // Waves saved before they could be drawn in steps were drawn smoothly, and before harmonics could be tuned, were
    // in tune. A silent harmonic's phase, which isn't saved, goes back to 0, so nothing from the sound before
    // lingers.
    drawings = {};

    for (auto& oscillator : oscillators)
    {
        oscillator.tunes = {};
        oscillator.phases = {};
    }

    for (const auto& wave : waves)
    {
        const waveform::Drawing drawing { (bool) wave.getProperty ("stepped", false),
                                          juce::jlimit (waveform::fewestSteps, waveform::mostSteps, (int) wave.getProperty ("steps", 8)) };
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
                drawings[(size_t) (numOscillators + lfo)] = drawing;
            }
        }
        else if (const auto oscillator = (int) wave.getProperty ("oscillator", 0); oscillator >= 0 && oscillator < numOscillators)
        {
            // Saves from before there were three oscillators have one wave, with no number.
            setShape (oscillator, restored, preset);
            drawings[(size_t) oscillator] = drawing;

            const auto tuneTexts = juce::StringArray::fromTokens (wave["tunes"].toString(), " ", {});

            for (int k = 0; k < std::min (tuneTexts.size(), waveform::maxHarmonics - 1); ++k)
                oscillators[(size_t) oscillator].tunes[(size_t) k + 1] = juce::jlimit (-waveform::furthestTune, waveform::furthestTune,
                                                                                      tuneTexts[k].getFloatValue());
        }
    }

    assignments = {};

    // A trigger's threshold, saved since it could be set. Saves from before trigger at the midline.
    const auto readThreshold = [] (const juce::ValueTree& tree)
    {
        return juce::jlimit (0.0f, 1.0f, (float) tree.getProperty ("threshold", 0.5f));
    };

    for (const auto& modulationTree : modulations)
    {
        const auto target = modulation::findTarget (modulationTree["target"].toString());

        // Saves from before there were envelopes number the LFO instead of naming the source.
        auto source = modulationTree.hasProperty ("lfo") ? (int) modulationTree["lfo"] : -1;

        for (int i = 0; i < modulation::numSources; ++i)
            if (modulationTree["source"].toString() == modulation::getSourceName (i))
                source = i;

        if (target.has_value() && source >= 0 && source < modulation::numSources
             && ! modulation::makesLoop (assignments, *target, source))
            assignments[(size_t) *target] = { source, juce::jlimit (0.0f, 1.0f, (float) modulationTree["range"]),
                                               (bool) modulationTree["inverted"],
                                               modulation::isEnvelope (source)
                                                   ? 0.0f
                                                   : juce::jlimit (0.0f, modulation::latestPhase, (float) modulationTree["phase"]),
                                               readThreshold (modulationTree) };
    }

    // Saves from before velocity could be assigned had it make softer notes quieter, as it does to start with.
    velocityAssignments = {};

    if (! velocityTree.isValid())
        velocityAssignments[(size_t) modulation::volumeTarget] = modulation::startingVelocityOnVolume;

    for (const auto& assignmentTree : velocityTree)
        if (const auto target = modulation::findTarget (assignmentTree["target"].toString()))
            velocityAssignments[(size_t) *target] = { modulation::velocitySource,
                                                      juce::jlimit (0.0f, 1.0f, (float) assignmentTree["range"]), false, 0.0f,
                                                      readThreshold (assignmentTree) };

    // Saves from before the mod wheel could be assigned have it move nothing.
    modWheelAssignments = {};

    for (const auto& assignmentTree : modWheelTree)
        if (const auto target = modulation::findTarget (assignmentTree["target"].toString()))
            modWheelAssignments[(size_t) *target] = { modulation::modWheelSource,
                                                      juce::jlimit (0.0f, 1.0f, (float) assignmentTree["range"]),
                                                      (bool) assignmentTree["inverted"], 0.0f, readThreshold (assignmentTree) };

    share();
    ++modulationVersion;

    // Loading changed everything, which on its own would count as editing the preset.
    presetName = name;
    presetEdited = edited;
    factoryPreset = factory;
    ++presetVersion;
    ++changeCount;
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
    markEdited();
}

//==============================================================================
void SynthProcessor::rememberForUndo()
{
    auto now = createState();

    if (! undoHistory.empty() && now.isEquivalentTo (undoHistory[undoStep]))
        return;

    // A change after undoing some takes the place of what was undone.
    if (! undoHistory.empty())
        undoHistory.resize (undoStep + 1);

    undoHistory.push_back (std::move (now));

    if (undoHistory.size() > mostUndoSteps)
        undoHistory.erase (undoHistory.begin());

    undoStep = undoHistory.size() - 1;
}

void SynthProcessor::updateUndoStep()
{
    if (undoHistory.empty())
        undoHistory.push_back (createState());
    else
        undoHistory[undoStep] = createState();
}

bool SynthProcessor::undo()
{
    if (! canUndo())
        return false;

    loadState (undoHistory[--undoStep].createCopy());
    return true;
}

bool SynthProcessor::redo()
{
    if (! canRedo())
        return false;

    loadState (undoHistory[++undoStep].createCopy());
    return true;
}

juce::AudioProcessorEditor* SynthProcessor::createEditor()
{
    return new SynthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthProcessor();
}
