#include "SynthProcessor.h"

#include <numeric>

#include "RandomWave.h"
#include "SynthEditor.h"

#include <BinaryData.h>

namespace
{
    constexpr int numVoices = 16;

    /** The waves the oscillators start with, so the first four sound different when they're switched on: a sine, a
        square, a sawtooth and a triangle, then a sine and a sawtooth again.
    */
    constexpr std::array<waveform::Preset, SynthProcessor::numOscillators> startingPresets {
        waveform::Preset::sine, waveform::Preset::square, waveform::Preset::sawtooth,
        waveform::Preset::triangle, waveform::Preset::sine, waveform::Preset::sawtooth
    };

    /** The waves and periods the LFOs start with. */
    constexpr std::array<waveform::Preset, SynthProcessor::numLfos> startingLfoPresets {
        waveform::Preset::sine, waveform::Preset::triangle, waveform::Preset::sawtooth, waveform::Preset::square,
        waveform::Preset::sine, waveform::Preset::triangle, waveform::Preset::sawtooth, waveform::Preset::square
    };

    constexpr std::array<float, SynthProcessor::numLfos> startingPeriods { 1.0f, 2.0f, 4.0f, 0.5f, 8.0f, 0.25f, 3.0f, 6.0f };

    /** The envelopes start as common shapes: a note that swells a little and settles, a pluck that dies away, a slow
        swell, a short blip, a gentle swell that holds, a long fade, a quick rise that falls to half, and a slow
        rise with a long tail.
    */
    constexpr std::array<EnvelopeSettings, SynthProcessor::numEnvelopes> startingEnvelopes { {
        { 0.01f, 0.3f, 0.7f, 0.5f },
        { 0.0f, 0.8f, 0.0f, 0.3f },
        { 2.0f, 1.0f, 1.0f, 2.0f },
        { 0.0f, 0.15f, 0.0f, 0.1f },
        { 0.5f, 0.5f, 0.8f, 1.0f },
        { 0.0f, 3.0f, 0.0f, 3.0f },
        { 0.05f, 0.6f, 0.5f, 0.8f },
        { 1.0f, 2.0f, 0.6f, 4.0f }
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
                        .withStringFromValueFunction ([] (float seconds, int) { return SynthProcessor::glideTimeToText (seconds); })),
                std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { SynthProcessor::noiseId (i), 1 }, name + " Noise", false)));
        }

        // The filters start switched out, a low pass, a high pass, a mid pass and a notch, at cutoffs that change the
        // sound clearly once they're switched in.
        layout.add (createFilterParameters (0, FilterType::lowPass, 1000.0f));
        layout.add (createFilterParameters (1, FilterType::highPass, 250.0f));
        layout.add (createFilterParameters (2, FilterType::midPass, 800.0f));
        layout.add (createFilterParameters (3, FilterType::notch, 1500.0f));

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
                                                              modulation::oneCycleABeat),
                std::make_unique<juce::AudioParameterBool> (juce::ParameterID { SynthProcessor::lfoStartOnPlayId (i), 1 },
                                                            name + " Start On Play", true)));
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

    /** A preset's category, from the start of its XML, where its outermost element's attributes are, without
        reading the rest, which can be long. Other if it has none, or one that isn't a category any more.
    */
    juce::String findCategory (const juce::String& xmlStart)
    {
        const auto element = xmlStart.fromFirstOccurrenceOf ("<EZSynth", false, false).upToFirstOccurrenceOf (">", false, false);
        const auto category = element.fromFirstOccurrenceOf ("presetCategory=\"", false, false).upToFirstOccurrenceOf ("\"", false, false);
        const auto unescaped = category.replace ("&amp;", "&").replace ("&quot;", "\"").replace ("&lt;", "<").replace ("&gt;", ">");
        return SynthProcessor::getPresetCategories().contains (unescaped) ? unescaped : juce::String (SynthProcessor::otherCategory);
    }

    /** A factory preset: its name, its category, and its XML, which is built into EZ Synth. */
    struct FactoryPreset
    {
        juce::String name;
        juce::String xml;
        juce::String category;
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
            {
                const auto xml = juce::String::fromUTF8 (data, size);
                presets.push_back ({ filename.dropLastCharacters (extension.length()), xml, findCategory (xml.substring (0, 4096)) });
            }
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
juce::String SynthProcessor::noiseId (int oscillator)       { return "osc" + juce::String (oscillator + 1) + "Noise"; }

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
juce::String SynthProcessor::lfoStartOnPlayId (int lfo)     { return "lfo" + juce::String (lfo + 1) + "StartOnPlay"; }
juce::String SynthProcessor::attackId (int envelope)        { return "env" + juce::String (envelope + 1) + "Attack"; }
juce::String SynthProcessor::decayId (int envelope)         { return "env" + juce::String (envelope + 1) + "Decay"; }
juce::String SynthProcessor::sustainId (int envelope)       { return "env" + juce::String (envelope + 1) + "Sustain"; }
juce::String SynthProcessor::releaseId (int envelope)       { return "env" + juce::String (envelope + 1) + "Release"; }

SynthProcessor::SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "EZSynth", createParameters())
{
    {
        const auto live = makeParameterSet ([this] (const juce::String& id) { return parameters.getRawParameterValue (id); });
        oscillatorParameters = live.oscillators;
        filterParameters = live.filters;
        lfoParameters = live.lfos;
        envelopeParameters = live.envelopes;
        volume = live.volume;
    }

    // Each pitch set's slot keeps its own copy of every parameter, starting as the host's do.
    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            parameterIndices[ranged->paramID] = (int) allParameters.size();
            allParameters.push_back (ranged);
            liveValues.push_back (parameters.getRawParameterValue (ranged->paramID));
        }
    }

    for (size_t slot = 0; slot < (size_t) maxPitchSets; ++slot)
    {
        slotValues[slot] = std::make_unique<std::atomic<float>[]> (allParameters.size());

        for (size_t i = 0; i < allParameters.size(); ++i)
            slotValues[slot][i] = liveValues[i]->load();

        slotParameters[slot] = makeParameterSet ([this, slot] (const juce::String& id)
        {
            return &slotValues[slot][(size_t) parameterIndices.at (id)];
        });
    }

    pitchSets.push_back ({});
    slotUsed[0] = true;
    receivedVersions.fill (-1);

    for (int target = 0; target < modulation::numTargets; ++target)
        if (modulation::isTrigger (target))
            triggerTargets.push_back (target);

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
        voices.push_back (dynamic_cast<HarmonicVoice*> (synth.addVoice (new HarmonicVoice (patches))));

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

    // No control is linked to a MIDI controller until one's learned, and one that's triggered is triggered as the
    // controller's turned right up.
    targetControllers.fill (-1);
    midiThresholds.fill (1.0f);

    // The audio thread asks for oscillators' harmonics to be randomized, and passes on MIDI controllers moved; the
    // message thread carries them out.
    startTimerHz (60);
}

SynthProcessor::ParameterSet SynthProcessor::makeParameterSet (const std::function<std::atomic<float>* (const juce::String&)>& find)
{
    ParameterSet set;
    set.volume = find (volumeId);

    for (int i = 0; i < numOscillators; ++i)
        set.oscillators[(size_t) i] = { find (onId (i)), find (levelId (i)), find (harmonicsId (i)), find (tuneId (i)),
                                        find (tuneZoomId (i)), find (glideId (i)), find (glideTimeId (i)),
                                        find (noiseId (i)) };

    for (int filter = 0; filter < modulation::numFilters; ++filter)
    {
        const auto ids = filterIds (filter);
        set.filters[(size_t) filter] = { find (ids.on), find (ids.type), find (ids.cutoff), find (ids.resonance),
                                         find (ids.width), find (ids.q), find (ids.depth) };
    }

    for (int i = 0; i < numLfos; ++i)
        set.lfos[(size_t) i] = { find (lfoOnId (i)), find (lfoPeriodId (i)), find (lfoModeId (i)), find (lfoRandomId (i)),
                                 find (lfoSyncId (i)), find (lfoBeatRateId (i)), find (lfoStartOnPlayId (i)) };

    for (int i = 0; i < numEnvelopes; ++i)
        set.envelopes[(size_t) i] = { find (attackId (i)), find (decayId (i)), find (sustainId (i)), find (releaseId (i)) };

    return set;
}

void SynthProcessor::copyParametersToSlot()
{
    auto& values = slotValues[(size_t) activeSlot.load()];

    for (size_t i = 0; i < liveValues.size(); ++i)
        values[i] = liveValues[i]->load();
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

    // The looping LFOs carry on where they were: a host may prepare again whenever its transport starts or loops,
    // and they're never sent back to the start of their cycles.
}

void SynthProcessor::restartLfos()
{
    lfoPhases = {};
    lfoCycles = {};
}

void SynthProcessor::startLfosOnPlay (std::optional<double> beat)
{
    // Each pitch set's LFOs, as it has them set.
    for (size_t slot = 0; slot < (size_t) maxPitchSets; ++slot)
    {
        if (! slotUsed[slot])
            continue;

        const auto& lfoSettings = slotParameters[slot].lfos;

        for (size_t i = 0; i < (size_t) numLfos; ++i)
        {
            if (lfoSettings[i].startOnPlay->load() < 0.5f)
                continue;

            // Synced, it lines up with the bar from wherever the host starts, as if it had run from the song's start;
            // otherwise, or without the host's position, it starts its cycle.
            const auto synced = lfoSettings[i].sync->load() >= 0.5f;
            const auto rate = juce::jlimit (0, (int) modulation::beatRates.size() - 1, juce::roundToInt (lfoSettings[i].beatRate->load()));
            const auto beatsPerCycle = 1.0 / modulation::beatRates[(size_t) rate];
            const auto periods = synced && beat.has_value() ? juce::jmax (0.0, *beat) / beatsPerCycle : 0.0;
            lfoCycles[slot][i] = (std::int64_t) std::floor (periods);
            lfoPhases[slot][i] = periods - std::floor (periods);
        }
    }
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
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            ++noteOnCount;
            lastNote = message.getNoteNumber();
            lastNoteOnSample = samplesPlayed + metadata.samplePosition;
        }

        // Where each controller's been put, for the controls linked to it, and the first moved while learning.
        if (message.isController() && message.getControllerNumber() < numLearnableControllers)
        {
            const auto number = (size_t) message.getControllerNumber();
            controllerValues[number] = message.getControllerValue();
            ++controllerMoves[number];

            if (learningTarget >= 0)
            {
                auto none = -1;
                learnedController.compare_exchange_strong (none, (int) number);
            }
        }
    }

    // The host's tempo, for LFOs synced to it, and where it starts playing from, for the LFOs that start then. Where
    // it is after that isn't followed, so when it loops back or jumps, the LFOs run on smoothly.
    playingTempo = modulation::defaultTempo;
    auto tempoFromHost = false;
    auto hostPlaying = false;
    std::optional<double> startingBeat;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
            {
                playingTempo = *bpm;
                tempoFromHost = true;
            }

            hostPlaying = position->getIsPlaying();

            if (const auto beat = position->getPpqPosition(); beat.hasValue())
                startingBeat = *beat;
        }
    }

    hostTempo = playingTempo;
    hostGivesTempo = tempoFromHost;

    if (hostPlaying && ! hostWasPlaying)
        startLfosOnPlay (startingBeat);

    hostWasPlaying = hostPlaying;

    for (int slot = 0; slot < maxPitchSets; ++slot)
        if (slotUsed[(size_t) slot])
            updatePatch (slot);

    // The LFOs are shown as the pitch set shown plays them: the newest note's, as of the last block, or the active one.
    const auto shown = getShownSlot();
    const auto& patch = patches.sets[shown];

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

    // Each pitch set's looping LFOs run on at its own rates. The mod wheel can change an LFO's rate, so it moves on
    // by the periods it's been through at that rate.
    for (size_t slot = 0; slot < (size_t) maxPitchSets; ++slot)
    {
        if (! slotUsed[slot])
            continue;

        for (size_t i = 0; i < (size_t) numLfos; ++i)
        {
            auto& phase = lfoPhases[slot][i];
            phase += numSamples / patches.sets[slot].getLfoPeriodSamples (i);
            const auto periodsDone = std::floor (phase);
            phase -= periodsDone;
            lfoCycles[slot][i] += (std::int64_t) periodsDone;
        }
    }

    // The window shows the active pitch set's LFOs.
    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        const auto& lfo = patch.lfos[i];
        lfoCyclesShown[i] = lfoCycles[shown][i];

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
            lfoProgress[i] = lfoPhases[shown][i];
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

    // Only the default pitch set's modulation triggers, the same whichever key is played, or none, with the newest
    // note's envelopes and velocity.
    const auto& patch = patches.sets[(size_t) defaultSlot];

    for (const auto triggerTarget : triggerTargets)
    {
        const auto target = (size_t) triggerTarget;

        if (! patch.assignments[target].isActive() && ! patch.velocityAssignments[target].isActive()
             && ! patch.modWheelAssignments[target].isActive())
            continue;

        auto& crossings = triggerCrossings[target];
        auto triggered = false;

        const auto watch = [this, &patch, &triggered, numSamples] (modulation::ThresholdCrossing& crossing, const modulation::Assignment& assignment)
        {
            const auto threshold = modulation::getThresholdValue (assignment.source, assignment.threshold);

            const auto isLfo = assignment.isActive() && assignment.source < numLfos;
            const auto looks = isLfo ? std::max (1, (numSamples + lookInterval - 1) / lookInterval) : 1;

            for (int look = looks - 1; look >= 0; --look)
            {
                const auto periodsEarlier = isLfo ? (double) (look * lookInterval) / patch.getLfoPeriodSamples ((size_t) assignment.source) : 0.0;
                const auto value = assignment.isActive() ? getSourceValueNow (assignment.source, assignment.phase, periodsEarlier, numSamples)
                                                         : std::nullopt;
                triggered = crossing.update (value, threshold, assignment.threshold) || triggered;
            }
        };

        watch (crossings[0], patch.assignments[target]);
        watch (crossings[1], patch.velocityAssignments[target]);
        watch (crossings[2], patch.modWheelAssignments[target]);

        if (triggered)
            ++triggerRequests[target];
    }
}

std::optional<float> SynthProcessor::getSourceValueNow (int source, double phase, double periodsEarlier, int numSamples) const
{
    // The default pitch set's LFOs, as it runs them.
    const auto& patch = patches.sets[(size_t) defaultSlot];

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

    // One that starts with each key counts from the last key pressed, and has nothing before the first; a looping
    // one is where the default pitch set's has got to, at the end of the block.
    if (lfo.fromKey && lastNoteOnSample < 0)
        return std::nullopt;

    const auto i = (size_t) source;
    const auto blockEnd = samplesPlayed + numSamples;

    // Before the LFO started, as the synth started playing or the key was pressed, there's nothing to look at.
    const auto sinceStart = (lfo.fromKey ? (double) (blockEnd - lastNoteOnSample) / patch.getLfoPeriodSamples (i)
                                         : (double) lfoCycles[(size_t) defaultSlot][i] + lfoPhases[(size_t) defaultSlot][i])
                          - periodsEarlier;

    if (sinceStart < 0.0)
        return std::nullopt;

    const auto periods = sinceStart - phase;

    if (! lfo.random)
        return modulation::lfoValue (lfo.shape, periods, lfo.fromKey, lfo.times);

    return lfo.fromKey ? randomwave::fromKeyValueAt (newestOnceSeeds[(size_t) source].load(), periods, lfo.drawing, lfo.times)
                       : randomwave::valueAt (lfo.seed, periods, lfo.drawing);
}

void SynthProcessor::randomizeAsTriggered()
{
    for (const auto target : triggerTargets)
    {
        const auto i = (size_t) target;
        const auto requests = triggerRequests[i].load();

        if (requests == triggerRequestsDone[i])
            continue;

        // A switch triggered more than once since last time ends up as each crossing leaves it, on or off.
        const auto times = requests - triggerRequestsDone[i];
        triggerRequestsDone[i] = requests;

        // Carried out in the default pitch set, whose modulation triggered it.
        if (modulation::describe (target).kind != modulation::Kind::randomize)
            for (int time = 0; time < (times % 2 == 0 ? 0 : 1); ++time)
                carryOutTrigger (target, defaultSlot);
        else
            carryOutTrigger (target, defaultSlot);
    }
}

void SynthProcessor::carryOutTrigger (int target, int slot)
{
    const auto info = modulation::describe (target);
    const auto pitchSet = std::find_if (pitchSets.begin(), pitchSets.end(), [slot] (const PitchSet& each) { return each.slot == slot; });

    // A pitch set taken away since it asked does nothing.
    if (pitchSet == pitchSets.end())
        return;

    // Another pitch set than the active one has its sound changed as it's kept: its oscillators, its copy of the
    // parameters and its saved sound, for when it's made active.
    if (slot != activeSlot.load())
    {
        const auto s = (size_t) slot;
        auto& sound = pitchSet->sound;

        if (info.kind == modulation::Kind::filterOn || info.kind == modulation::Kind::oscillatorOn)
        {
            const auto key = modulation::getKey (target);
            const auto index = (size_t) parameterIndices.at (key);
            const auto value = slotValues[s][index].load() >= 0.5f ? 0.0f : 1.0f;
            slotValues[s][index] = value;

            if (auto parameter = sound.getChildWithProperty ("id", key); parameter.isValid())
                parameter.setProperty ("value", value, nullptr);
        }
        else
        {
            auto& oscillator = slotOscillators[s][(size_t) info.oscillator];

            if (info.kind == modulation::Kind::harmonicSwitch)
            {
                using Switch = waveform::HarmonicSwitch;
                const auto numHarmonics = juce::roundToInt (slotParameters[s].oscillators[(size_t) info.oscillator].numHarmonics->load());
                auto& state = oscillator.switches[(size_t) info.harmonic - 1];

                if (waveform::HarmonicSet (oscillator.switches).plays (info.harmonic, numHarmonics))
                    state = Switch::off;
                else
                    state = info.harmonic <= numHarmonics ? Switch::follows : Switch::on;
            }
            else if (info.kind == modulation::Kind::randomize)
            {
                randomizeOscillator (oscillator, randomizer);
                ++randomizeCounts[(size_t) info.oscillator];
            }

            shareOscillators (s, slotOscillators[s]);

            for (auto wave : sound)
                if (wave.hasType ("WAVE") && wave.hasProperty ("oscillator") && (int) wave["oscillator"] == info.oscillator)
                    writeOscillator (wave, oscillator);
        }

        markEdited();
        ++shapeVersion;
        return;
    }

    if (info.kind == modulation::Kind::harmonicSwitch)
    {
        toggleHarmonic (info.oscillator, info.harmonic);
    }
    else if (info.kind == modulation::Kind::filterOn || info.kind == modulation::Kind::oscillatorOn)
    {
        if (auto* parameter = parameters.getParameter (modulation::getKey (target)))
            parameter->setValueNotifyingHost (parameter->getValue() >= 0.5f ? 0.0f : 1.0f);
    }
    else if (info.kind == modulation::Kind::randomize)
    {
        randomizeHarmonics (info.oscillator, randomizer);
        ++randomizeCounts[(size_t) info.oscillator];
    }
}

void SynthProcessor::timerCallback()
{
    randomizeAsTriggered();
    applyMidiControllers();
}

//==============================================================================
void SynthProcessor::startMidiLearn (int target)
{
    learnedController = -1;
    learningTarget = target;
    ++midiLearnVersion;
}

void SynthProcessor::stopMidiLearn()
{
    learningTarget = -1;
    learnedController = -1;
    ++midiLearnVersion;
}

std::optional<int> SynthProcessor::getMidiLearnTarget() const noexcept
{
    const auto target = learningTarget.load();
    return target >= 0 ? std::optional (target) : std::nullopt;
}

std::optional<int> SynthProcessor::getMidiController (int target) const noexcept
{
    const auto controller = targetControllers[(size_t) target];
    return controller >= 0 ? std::optional (controller) : std::nullopt;
}

void SynthProcessor::setMidiController (int target, std::optional<int> controller)
{
    // A controller sets one control, so linking it to this one unlinks it from any other.
    if (controller.has_value())
        for (auto& linked : targetControllers)
            if (linked == *controller)
                linked = -1;

    targetControllers[(size_t) target] = controller.has_value() && *controller >= 0 && *controller < numLearnableControllers ? *controller : -1;

    // The control waits for its controller to be moved before following it.
    if (controller.has_value())
        appliedMoves[(size_t) target] = controllerMoves[(size_t) *controller].load();

    ++midiLearnVersion;
}

void SynthProcessor::setMidiThreshold (int target, float threshold)
{
    midiThresholds[(size_t) target] = juce::jlimit (0.0f, 1.0f, threshold);
    ++midiLearnVersion;
}

void SynthProcessor::applyMidiControllers()
{
    // The controller moved to teach a control sets it straight away.
    if (const auto target = learningTarget.load(); target >= 0)
    {
        if (const auto controller = learnedController.load(); controller >= 0)
        {
            setMidiController (target, controller);
            appliedMoves[(size_t) target] = controllerMoves[(size_t) controller].load() - 1;
            stopMidiLearn();
        }
    }

    for (size_t target = 0; target < targetControllers.size(); ++target)
    {
        const auto controller = targetControllers[target];

        if (controller < 0)
            continue;

        if (const auto moves = controllerMoves[(size_t) controller].load(); moves != appliedMoves[target])
        {
            appliedMoves[target] = moves;
            setTargetPosition ((int) target, (float) controllerValues[(size_t) controller].load() / 127.0f);
        }
    }
}

void SynthProcessor::setTargetPosition (int target, float position)
{
    position = juce::jlimit (0.0f, 1.0f, position);
    const auto info = modulation::describe (target);

    // Randomizing, and an oscillator's, filter's or harmonic's switch, are triggered by the controller crossing its threshold, either way.
    if (modulation::isTrigger (target))
    {
        const auto threshold = getMidiThreshold (target);

        if (controllerTriggerCrossings[(size_t) target].update (position, modulation::getThresholdValue (-1, threshold),
                                                                                threshold))
            carryOutTrigger (target, activeSlot.load());

        return;
    }

    if (info.kind == modulation::Kind::harmonic)
    {
        setHarmonicAmplitude (info.oscillator, info.harmonic, modulation::amplitudeRange().convertFrom0to1 (position));
        return;
    }

    if (info.kind == modulation::Kind::harmonicTune)
    {
        setHarmonicTune (info.oscillator, info.harmonic, (position - 0.5f) * 2.0f * waveform::furthestTune);
        return;
    }

    if (info.kind == modulation::Kind::harmonicPhase)
    {
        setHarmonicPhase (info.oscillator, info.harmonic, position * juce::MathConstants<float>::twoPi);
        return;
    }

    // A synced LFO's rate dial sets the cycles a beat.
    const auto id = info.kind == modulation::Kind::lfoRate && isLfoSynced (info.lfo) ? lfoBeatRateId (info.lfo)
                                                                                     : modulation::getKey (target);

    if (auto* parameter = parameters.getParameter (id))
        parameter->setValueNotifyingHost (position);
}

void SynthProcessor::shareNewestNote()
{
    const HarmonicVoice* newest = nullptr;

    for (const auto* voice : voices)
        if (voice->isVoiceActive() && (newest == nullptr || voice->getSamplesSinceNoteOn() < newest->getSamplesSinceNoteOn()))
            newest = voice;

    noteSounding = newest != nullptr;

    // The noise the newest note played, if the audio thread can have it without waiting for the window.
    {
        const juce::SpinLock::ScopedTryLockType lock (noiseLock);

        if (lock.isLocked())
        {
            for (size_t i = 0; i < (size_t) numOscillators; ++i)
            {
                noisePlayed[i] = newest != nullptr && newest->getPatch().oscillators[i].noise && newest->getPatch().oscillators[i].on;

                if (noisePlayed[i])
                    newest->copyRecentNoise ((int) i, playedNoise[i].data(), noiseSamples);
            }

            ++noiseCount;
        }
    }

    if (newest == nullptr)
    {
        shownSlot = activeSlot.load();
        return;
    }

    shownSlot = (int) (&newest->getPatch() - patches.sets.data());

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
        display.progress = run.getProgress (newest->getPatch().envelopes[(size_t) i]);
        display.level = run.getLevel();
        display.releaseFrom = run.getReleaseFrom();
    }
}

void SynthProcessor::updatePatch (int slotNumber)
{
    const auto slot = (size_t) slotNumber;
    auto& patch = patches.sets[slot];
    auto& got = received[slot];

    if (sharedVersions[slot].load() != receivedVersions[slot])
    {
        auto taken = false;

        {
            const juce::SpinLock::ScopedTryLockType lock (sharedLocks[slot]);

            if (lock.isLocked())
            {
                got = shared[slot];
                receivedVersions[slot] = sharedVersions[slot].load();
                taken = true;
            }
        }

        if (taken)
        {
            for (size_t i = 0; i < (size_t) numOscillators; ++i)
            {
                auto& oscillator = patch.oscillators[i];
                const auto& decomposition = got.decompositions[i];

                for (size_t k = 0; k < (size_t) waveform::maxHarmonics; ++k)
                {
                    // A harmonic the wave hasn't got, if an LFO raises it, comes in at the phase it last had.
                    const auto& harmonic = decomposition.harmonics[k];
                    const auto phase = harmonic.amplitude > 0.0f ? harmonic.phase : got.phases[i][k];
                    oscillator.amplitudes[k] = harmonic.amplitude;
                    oscillator.phases[k] = phase;
                    oscillator.phaseSines[k] = std::sin (phase);
                    oscillator.phaseCosines[k] = std::cos (phase);
                }

                oscillator.peaks = got.peaks[i];
                oscillator.switches = got.switches[i];
                oscillator.tunes = got.tunes[i];
            }

            for (size_t i = 0; i < (size_t) numLfos; ++i)
            {
                patch.lfos[i].shape = got.lfoShapes[i];
                patch.lfos[i].drawing = got.lfoDrawings[i];
            }

            patch.assignments = got.assignments;
            patch.velocityAssignments = got.velocityAssignments;
            patch.modWheelAssignments = got.modWheelAssignments;
        }
    }

    const auto& settings = slotParameters[slot];

    for (size_t i = 0; i < (size_t) numOscillators; ++i)
    {
        const auto& source = settings.oscillators[i];
        auto& oscillator = patch.oscillators[i];
        oscillator.on = source.on->load() >= 0.5f;
        oscillator.level = source.level->load();
        oscillator.numHarmonics = juce::jlimit (1, waveform::maxHarmonics, juce::roundToInt (source.numHarmonics->load()));
        oscillator.tune = source.tune->load();
        oscillator.tuneReach = tuning::zoomToReach (source.tuneZoom->load());
        oscillator.glide = (Patch::Glide) juce::jlimit (0, 2, juce::roundToInt (source.glide->load()));
        oscillator.glideSeconds = source.glideTime->load();
        oscillator.noise = source.noise->load() >= 0.5f;
    }

    for (size_t i = 0; i < (size_t) modulation::numFilters; ++i)
    {
        const auto& filter = settings.filters[i];
        patch.filters[i] = { filter.on->load() >= 0.5f,
                             (FilterType) juce::jlimit (0, (int) allFilterTypes.size() - 1, juce::roundToInt (filter.type->load())),
                             filter.cutoff->load(), filter.resonance->load(), filter.width->load(), filter.q->load(),
                             filter.depth->load() };
    }

    patch.volume = settings.volume->load();

    for (size_t i = 0; i < (size_t) numEnvelopes; ++i)
    {
        const auto& envelope = settings.envelopes[i];
        patch.envelopes[i] = { envelope.attack->load(), envelope.decay->load(), envelope.sustain->load(), envelope.release->load() };
    }

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        const auto& source = settings.lfos[i];
        auto& lfo = patch.lfos[i];
        lfo.on = source.on->load() >= 0.5f;
        const auto mode = juce::jlimit (0, modulation::lfoForever, juce::roundToInt (source.mode->load()));
        lfo.fromKey = mode != modulation::lfoLoops;
        lfo.times = modulation::getLfoTimes (mode);
        lfo.synced = source.sync->load() >= 0.5f;

        // Synced to the tempo, it runs a number of cycles a beat, carrying on from where it is as the tempo changes.
        if (lfo.synced)
        {
            const auto rate = juce::jlimit (0, (int) modulation::beatRates.size() - 1, juce::roundToInt (source.beatRate->load()));
            lfo.periodSeconds = (float) (60.0 / (modulation::beatRates[(size_t) rate] * playingTempo));
        }
        else
        {
            lfo.periodSeconds = source.period->load();
        }

        lfo.periodSamples = std::max (1.0, lfo.periodSeconds * playingSampleRate);
        lfo.phase = lfoPhases[slot][i];
        lfo.cycle = lfoCycles[slot][i];
        lfo.random = source.random->load() >= 0.5f;
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

static_assert (SynthProcessor::noiseSamples == HarmonicVoice::recentNoiseLength);

bool SynthProcessor::isNoise (int oscillator) const noexcept
{
    return oscillatorParameters[(size_t) oscillator].noise->load() >= 0.5f;
}

std::vector<float> SynthProcessor::getPlayedNoise (int oscillator) const
{
    const juce::SpinLock::ScopedLockType lock (noiseLock);
    const auto o = (size_t) oscillator;

    if (! noisePlayed[o])
        return {};

    return { playedNoise[o].begin(), playedNoise[o].end() };
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

void SynthProcessor::applyShape (Oscillator& changed, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    changed.shape = newShape;
    changed.preset = fromPreset;
    changed.decomposition = waveform::decompose (newShape);

    for (const auto& harmonic : changed.decomposition.harmonics)
        if (harmonic.amplitude > 0.0f)
            changed.phases[(size_t) harmonic.harmonic - 1] = harmonic.phase;
}

void SynthProcessor::replaceShape (int oscillator, const waveform::Shape& newShape, std::optional<waveform::Preset> fromPreset)
{
    applyShape (oscillators[(size_t) oscillator], newShape, fromPreset);
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

waveform::HarmonicSwitch SynthProcessor::getHarmonicSwitch (int oscillator, int harmonic) const noexcept
{
    if (harmonic < 1 || harmonic > waveform::maxHarmonics)
        return waveform::HarmonicSwitch::follows;

    return oscillators[(size_t) oscillator].switches[(size_t) harmonic - 1];
}

void SynthProcessor::setHarmonicSwitch (int oscillator, int harmonic, waveform::HarmonicSwitch state)
{
    if (harmonic < 1 || harmonic > waveform::maxHarmonics)
        return;

    markEdited();
    oscillators[(size_t) oscillator].switches[(size_t) harmonic - 1] = state;
    share();
    ++shapeVersion;
}

bool SynthProcessor::isHarmonicPlayed (int oscillator, int harmonic) const noexcept
{
    return waveform::HarmonicSet (oscillators[(size_t) oscillator].switches).plays (harmonic, getNumHarmonics (oscillator));
}

void SynthProcessor::toggleHarmonic (int oscillator, int harmonic)
{
    using Switch = waveform::HarmonicSwitch;

    if (isHarmonicPlayed (oscillator, harmonic))
        setHarmonicSwitch (oscillator, harmonic, Switch::off);
    else
        setHarmonicSwitch (oscillator, harmonic, harmonic <= getNumHarmonics (oscillator) ? Switch::follows : Switch::on);
}

void SynthProcessor::cycleHarmonicSwitch (int oscillator, int harmonic)
{
    using Switch = waveform::HarmonicSwitch;

    switch (getHarmonicSwitch (oscillator, harmonic))
    {
        case Switch::follows:  setHarmonicSwitch (oscillator, harmonic, Switch::off); break;
        case Switch::off:      setHarmonicSwitch (oscillator, harmonic, Switch::on); break;
        case Switch::on:       setHarmonicSwitch (oscillator, harmonic, Switch::follows); break;
    }
}

void SynthProcessor::randomizeHarmonics (int oscillator, juce::Random& random)
{
    markEdited();
    randomizeOscillator (oscillators[(size_t) oscillator], random);
    share();
    ++shapeVersion;
}

void SynthProcessor::randomizeOscillator (Oscillator& oscillator, juce::Random& random)
{
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

    applyShape (oscillator, shape, std::nullopt);

    // Harmonics too weak to keep come back in at their new phases when they're raised.
    std::copy (phases.begin(), phases.end(), oscillator.phases.begin());
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

bool SynthProcessor::isLfoStartedOnPlay (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].startOnPlay->load() >= 0.5f;
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
    const auto mode = getShownLfoMode (lfo);
    const auto times = modulation::getLfoTimes (mode);
    return mode != modulation::lfoLoops && times > 0 && getLfoProgress (lfo).value_or (0.0) >= (double) (times - 1);
}

std::optional<double> SynthProcessor::getLfoProgress (int lfo) const noexcept
{
    const auto progress = lfoProgress[(size_t) lfo].load();
    return progress >= 0.0 ? std::optional (progress) : std::nullopt;
}

int SynthProcessor::getShownPitchSet() const
{
    for (size_t i = 0; i < pitchSets.size(); ++i)
        if (pitchSets[i].slot == shownSlot.load())
            return (int) i;

    return activePitchSet;
}

int SynthProcessor::getShownLfoMode (int lfo) const noexcept
{
    return juce::jlimit (0, modulation::lfoForever, juce::roundToInt (getShownParameters().lfos[(size_t) lfo].mode->load()));
}

bool SynthProcessor::isShownLfoRandom (int lfo) const noexcept
{
    return getShownParameters().lfos[(size_t) lfo].random->load() >= 0.5f;
}

float SynthProcessor::getLfoValue (int lfo, float phase) const
{
    // As the pitch set shown plays it.
    const auto progress = getLfoProgress (lfo).value_or (0.0) - phase;
    const auto mode = getShownLfoMode (lfo);
    const auto fromKey = mode != modulation::lfoLoops;
    const auto times = modulation::getLfoTimes (mode);
    const auto& shown = getShownShared();

    if (! isShownLfoRandom (lfo))
        return modulation::lfoValue (shown.lfoShapes[(size_t) lfo], progress, fromKey, times);

    const auto random = getRandomCycle (lfo);
    const auto& drawing = shown.lfoDrawings[(size_t) lfo];

    if (fromKey)
        return randomwave::fromKeyValueAt (random.seed, progress, drawing, times);

    return randomwave::valueAt (random.seed, (double) random.cycle + progress, drawing);
}

bool SynthProcessor::isLfoRandom (int lfo) const noexcept
{
    return lfoParameters[(size_t) lfo].random->load() >= 0.5f;
}

SynthProcessor::RandomCycle SynthProcessor::getRandomCycle (int lfo) const noexcept
{
    if (const auto mode = getShownLfoMode (lfo); mode != modulation::lfoLoops)
    {
        // The cycle the newest note is in, staying on the last once an LFO that plays a number of times has finished.
        const auto times = modulation::getLfoTimes (mode);
        auto cycle = (std::int64_t) std::floor (std::max (0.0, getLfoProgress (lfo).value_or (0.0)));

        if (times > 0)
            cycle = std::min (cycle, (std::int64_t) times - 1);

        return { newestOnceSeeds[(size_t) lfo].load(), cycle };
    }

    return { randomSeeds[(size_t) lfo], lfoCyclesShown[(size_t) lfo].load() };
}

waveform::Shape SynthProcessor::getPlayedLfoShape (int lfo) const
{
    // As the pitch set shown plays it.
    const auto& shown = getShownShared();

    if (! isShownLfoRandom (lfo))
        return shown.lfoShapes[(size_t) lfo];

    const auto random = getRandomCycle (lfo);
    return randomwave::getCycle (random.seed, random.cycle, shown.lfoDrawings[(size_t) lfo]);
}

std::optional<float> SynthProcessor::getPlayedLfoEnd (int lfo) const
{
    if (! isShownLfoRandom (lfo) || isLfoOnLastTime (lfo))
        return std::nullopt;

    const auto random = getRandomCycle (lfo);
    return randomwave::valueAt (random.seed, (double) random.cycle + 1.0, getShownShared().lfoDrawings[(size_t) lfo]);
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

    if (getShownParameters().lfos[(size_t) source].on->load() >= 0.5f && getLfoProgress (source).has_value())
        return getLfoValue (source, phase);

    return std::nullopt;
}

std::optional<float> SynthProcessor::getLivePosition (int target) const
{
    // As the pitch set shown plays it, which may not be the active one.
    const auto& shown = getShownShared();
    const auto& settings = getShownParameters();

    // A synced LFO's rate is the tempo's, which nothing moves, and randomizing is triggered, not moved.
    if (const auto info = modulation::describe (target); (info.kind == modulation::Kind::lfoRate && settings.lfos[(size_t) info.lfo].sync->load() >= 0.5f)
                                                           || modulation::isTrigger (target))
        return std::nullopt;

    // Moved as the voices move it (HarmonicVoice::getTargets): by velocity and the mod wheel, then by the LFO or envelope,
    // each across less of an oscillator's tune when its slider's zoomed in. Where another pitch set than the active
    // one is shown, a control set differently there counts as moved, to where that pitch set has it.
    auto position = getShownPosition (target);
    auto moved = ! isShownActive() && std::abs (position - getPosition (target)) > 1.0e-4f;
    const auto info = modulation::describe (target);
    const auto scale = info.kind == modulation::Kind::oscillatorTune
                           ? tuning::getModulationScale (tuning::zoomToReach (settings.oscillators[(size_t) info.oscillator].tuneZoom->load()))
                           : 1.0f;

    if (const auto& byVelocity = shown.velocityAssignments[(size_t) target]; byVelocity.isActive())
    {
        if (const auto velocity = getSourceValue (modulation::velocitySource))
        {
            position += scale * byVelocity.offset (*velocity);
            moved = true;
        }
    }

    if (const auto& byModWheel = shown.modWheelAssignments[(size_t) target]; byModWheel.isActive())
    {
        position += scale * byModWheel.offset (modWheelShown.load());
        moved = true;
    }

    if (const auto& assignment = shown.assignments[(size_t) target]; assignment.isActive())
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
    // Moved as the voices move them (HarmonicVoice::getTargets), in the pitch set shown. Another pitch set than the
    // active one counts as moved, as its harmonics aren't the ones the window shows set.
    const auto& shown = getShownShared();
    const auto o = (size_t) oscillator;
    LiveWave wave;
    wave.moved = ! isShownActive();
    auto numHarmonics = juce::roundToInt (getShownParameters().oscillators[o].numHarmonics->load());

    if (const auto moved = getLivePosition (modulation::oscillatorHarmonicsTarget (oscillator)))
    {
        numHarmonics = modulation::positionToHarmonics (*moved);
        wave.moved = true;
    }

    const auto& harmonics = shown.decompositions[o].harmonics;
    const auto& switches = shown.switches[o];

    for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
    {
        if (! switches.plays (harmonic, numHarmonics))
            continue;

        const auto k = (size_t) harmonic - 1;
        auto partial = harmonics[k];
        partial.tune = harmonic > 1 ? shown.tunes[o][k] : 0.0f;
        const auto phase = partial.amplitude > 0.0f ? partial.phase : shown.phases[o][k];
        partial.phase = phase - juce::MathConstants<float>::twoPi * std::floor (phase / juce::MathConstants<float>::twoPi);

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

float SynthProcessor::getShownPosition (int target) const
{
    const auto slot = getShownSlot();

    if (modulation::isParameter (target))
    {
        const auto index = (size_t) parameterIndices.at (modulation::getKey (target));
        return allParameters[index]->convertTo0to1 (slotValues[slot][index].load());
    }

    if (modulation::isTrigger (target))
        return 0.0f;

    const auto info = modulation::describe (target);
    const auto& shown = shared[slot];
    const auto o = (size_t) info.oscillator;
    const auto k = (size_t) info.harmonic - 1;

    if (info.kind == modulation::Kind::harmonicTune)
        return shown.tunes[o][k] / (2.0f * waveform::furthestTune) + 0.5f;

    if (info.kind == modulation::Kind::harmonicPhase)
    {
        const auto& harmonic = shown.decompositions[o].harmonics[k];
        const auto phase = harmonic.amplitude > 0.0f ? harmonic.phase : shown.phases[o][k];
        const auto turn = juce::MathConstants<float>::twoPi;
        return (phase - turn * std::floor (phase / turn)) / turn;
    }

    return modulation::amplitudeRange().convertTo0to1 (shown.decompositions[o].harmonics[k].amplitude);
}

float SynthProcessor::getPosition (int target) const
{
    if (modulation::isParameter (target))
        return parameters.getParameter (modulation::getKey (target))->getValue();

    const auto info = modulation::describe (target);

    // Randomizing, or switching a harmonic, isn't anywhere along a travel.
    if (modulation::isTrigger (target))
        return 0.0f;

    // A harmonic's tune is in tune in the middle of its travel, and its phase goes from 0 round to a whole turn.
    if (info.kind == modulation::Kind::harmonicTune)
        return getHarmonicTune (info.oscillator, info.harmonic) / (2.0f * waveform::furthestTune) + 0.5f;

    if (info.kind == modulation::Kind::harmonicPhase)
        return getHarmonicPhase (info.oscillator, info.harmonic) / juce::MathConstants<float>::twoPi;

    const auto& harmonic = getDecomposition (info.oscillator).harmonics[(size_t) info.harmonic - 1];
    return modulation::amplitudeRange().convertTo0to1 (harmonic.amplitude);
}

namespace
{
    template <typename Shared, typename Oscillators>
    void shareOscillatorsInto (Shared& sharing, const Oscillators& oscillators)
    {
        for (size_t i = 0; i < oscillators.size(); ++i)
        {
            sharing.decompositions[i] = oscillators[i].decomposition;
            sharing.phases[i] = oscillators[i].phases;
            sharing.tunes[i] = oscillators[i].tunes;
            sharing.switches[i] = waveform::HarmonicSet (oscillators[i].switches);
            sharing.peaks[i] = waveform::getPeaks (oscillators[i].decomposition, sharing.switches[i]);
        }
    }
}

void SynthProcessor::shareOscillators (size_t slot, const std::array<Oscillator, numOscillators>& from)
{
    const juce::SpinLock::ScopedLockType lock (sharedLocks[slot]);
    shareOscillatorsInto (shared[slot], from);
    ++sharedVersions[slot];
}

void SynthProcessor::writeOscillator (juce::ValueTree& wave, const Oscillator& oscillator)
{
    wave.setProperty ("points", juce::MemoryBlock (oscillator.shape.data(), sizeof (oscillator.shape)).toBase64Encoding(), nullptr);
    wave.setProperty ("preset", oscillator.preset.has_value() ? waveform::getName (*oscillator.preset) : juce::String(), nullptr);

    // How far each harmonic from the 2nd up is tuned, saved only if any is.
    const auto& tunes = oscillator.tunes;
    wave.removeProperty ("tunes", nullptr);

    if (std::any_of (tunes.begin(), tunes.end(), [] (float tune) { return ! juce::exactlyEqual (tune, 0.0f); }))
    {
        juce::StringArray tuneTexts;

        for (size_t k = 1; k < tunes.size(); ++k)
            tuneTexts.add (juce::String (tunes[k]));

        wave.setProperty ("tunes", tuneTexts.joinIntoString (" "), nullptr);
    }

    // Which harmonics are switched off or on, from the 1st up, saved only if any is: 0 follows the number of
    // harmonics, 1 is off and 2 on.
    const auto& switches = oscillator.switches;
    wave.removeProperty ("switches", nullptr);

    if (std::any_of (switches.begin(), switches.end(), [] (waveform::HarmonicSwitch each) { return each != waveform::HarmonicSwitch::follows; }))
    {
        juce::String switchTexts;

        for (const auto each : switches)
            switchTexts << juce::String ((int) each);

        wave.setProperty ("switches", switchTexts, nullptr);
    }
}

void SynthProcessor::share()
{
    const auto slot = (size_t) activeSlot.load();
    const juce::SpinLock::ScopedLockType lock (sharedLocks[slot]);
    auto& sharing = shared[slot];
    shareOscillatorsInto (sharing, oscillators);

    for (size_t i = 0; i < (size_t) numLfos; ++i)
    {
        sharing.lfoShapes[i] = lfos[i].shape;
        sharing.lfoDrawings[i] = drawings[(size_t) numOscillators + i];
    }

    sharing.assignments = assignments;
    sharing.velocityAssignments = velocityAssignments;
    sharing.modWheelAssignments = modWheelAssignments;
    ++sharedVersions[slot];
}

//==============================================================================
void SynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // The MIDI learn links go with a host's project, but not into presets or the undo history.
    auto state = createState();
    juce::ValueTree links ("MIDILEARN");

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto controller = getMidiController (target))
        {
            juce::ValueTree link ("LINK");
            link.setProperty ("target", modulation::getKey (target), nullptr);
            link.setProperty ("controller", *controller, nullptr);

            if (modulation::isTrigger (target))
                link.setProperty ("threshold", getMidiThreshold (target), nullptr);
            links.appendChild (link, nullptr);
        }
    }

    state.appendChild (links, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    // A project saved before MIDI learn has no links, and keeps those learned since.
    if (const auto links = state.getChildWithName ("MIDILEARN"); links.isValid())
    {
        targetControllers.fill (-1);
        midiThresholds.fill (1.0f);

        // Links saved before they had thresholds were triggered halfway.
        for (const auto& link : links)
        {
            if (const auto target = modulation::findTarget (link["target"].toString()))
            {
                setMidiController (*target, (int) link["controller"]);
                setMidiThreshold (*target, (float) link.getProperty ("threshold", 0.5f));
            }
        }

        state.removeChild (links, nullptr);
    }

    loadState (state);
}

juce::ValueTree SynthProcessor::createState()
{
    auto state = createSoundState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetEdited", presetEdited.load(), nullptr);
    state.setProperty ("presetFactory", factoryPreset, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);

    // A sound with pitch sets lists them all, in order, with the sound of each but the active one, whose sound is
    // the state's own. A sound with only the default has no list, as before there were pitch sets.
    if (pitchSets.size() > 1)
    {
        juce::ValueTree list ("PITCHSETS");
        list.setProperty ("active", activePitchSet, nullptr);

        for (size_t i = 0; i < pitchSets.size(); ++i)
        {
            const auto& pitchSet = pitchSets[i];
            juce::ValueTree entry ("PITCHSET");

            if (pitchSet.range.has_value())
            {
                entry.setProperty ("lowest", pitchSet.range->lowest, nullptr);
                entry.setProperty ("highest", pitchSet.range->highest, nullptr);
            }

            if ((int) i != activePitchSet)
                entry.appendChild (pitchSet.sound.createCopy(), nullptr);

            list.appendChild (entry, nullptr);
        }

        state.appendChild (list, nullptr);
    }

    return state;
}

juce::ValueTree SynthProcessor::createSoundState()
{
    auto state = parameters.copyState();
    state.setProperty ("beatRates", (int) modulation::beatRates.size(), nullptr);
    state.setProperty ("tuneReach", tuning::widestReach, nullptr);

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
        writeOscillator (wave, oscillators[(size_t) i]);
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
    auto category = state.getProperty ("presetCategory", otherCategory).toString();
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetEdited", nullptr);
    state.removeProperty ("presetFactory", nullptr);
    state.removeProperty ("presetCategory", nullptr);

    if (! getPresetCategories().contains (category))
        category = otherCategory;

    // The pitch sets, each in its own slot. A save from before there were pitch sets has only the default.
    const auto list = state.getChildWithName ("PITCHSETS");
    state.removeChild (list, nullptr);

    std::vector<PitchSet> loaded;
    std::vector<juce::ValueTree> sounds;
    auto active = 0;

    if (list.isValid())
    {
        for (const auto& entry : list)
        {
            if (! entry.hasType ("PITCHSET") || (int) loaded.size() >= maxPitchSets)
                continue;

            PitchSet pitchSet;

            // The first is the default, which has no range; any other without one is left out.
            if (! loaded.empty())
            {
                if (! entry.hasProperty ("lowest") || ! entry.hasProperty ("highest"))
                    continue;

                const auto a = juce::jlimit (0, 127, (int) entry["lowest"]);
                const auto b = juce::jlimit (0, 127, (int) entry["highest"]);
                pitchSet.range = KeyRange { std::min (a, b), std::max (a, b) };
            }

            if ((int) entry.getParent().indexOf (entry) == (int) list["active"])
                active = (int) loaded.size();

            pitchSet.slot = (int) loaded.size();
            loaded.push_back (pitchSet);
            sounds.push_back (entry.getChild (0));
        }
    }

    if (loaded.empty())
    {
        loaded.push_back ({});
        sounds.push_back ({});
    }

    // Each pitch set but the active one is loaded in turn, filling its slot, then the active one, from the state.
    pitchSets = loaded;

    for (size_t slot = 0; slot < (size_t) maxPitchSets; ++slot)
        slotUsed[slot] = slot < pitchSets.size();

    for (size_t i = 0; i < pitchSets.size(); ++i)
    {
        if ((int) i == active)
            continue;

        activePitchSet = (int) i;
        activeSlot = pitchSets[i].slot;
        loadSoundState (sounds[i].isValid() ? sounds[i].createCopy() : initState.createCopy());
        pitchSets[i].sound = createSoundState();
        slotOscillators[(size_t) pitchSets[i].slot] = oscillators;
    }

    activePitchSet = active;
    activeSlot = pitchSets[(size_t) active].slot;
    loadSoundState (state);
    updateKeyMap();
    ++pitchSetVersion;

    // Loading changed everything, which on its own would count as editing the preset.
    presetName = name;
    presetEdited = edited;
    factoryPreset = factory;
    presetCategory = category;
    ++presetVersion;
    ++changeCount;
}

void SynthProcessor::loadSoundState (juce::ValueTree state)
{
    state.removeProperty ("presetCategory", nullptr);
    state.removeProperty ("presetName", nullptr);
    state.removeProperty ("presetEdited", nullptr);
    state.removeProperty ("presetFactory", nullptr);
    state.removeChild (state.getChildWithName ("PITCHSETS"), nullptr);

    // A save from before the slowest beat rates were added counts its LFOs' beat rates from 1/8 a beat, so each
    // moves up by as many as were added below it.
    const auto ratesAdded = (int) modulation::beatRates.size() - (int) state.getProperty ("beatRates", 11);
    state.removeProperty ("beatRates", nullptr);

    for (int i = 0; i < numLfos && ratesAdded > 0; ++i)
        if (auto rate = state.getChildWithProperty ("id", lfoBeatRateId (i)); rate.isValid())
            rate.setProperty ("value", (double) rate["value"] + ratesAdded, nullptr);

    // A save from before oscillators could be tuned two octaves away has its tunes' zooms counted from an octave
    // either side, so each is moved to where it reaches as far as it did.
    const auto oldReach = juce::approximatelyEqual ((float) state.getProperty ("tuneReach", tuning::oldWidestReach), tuning::oldWidestReach);
    state.removeProperty ("tuneReach", nullptr);

    for (int i = 0; i < numOscillators && oldReach; ++i)
        if (auto zoom = state.getChildWithProperty ("id", tuneZoomId (i)); zoom.isValid())
            zoom.setProperty ("value", tuning::updateOldZoom ((float) zoom["value"]), nullptr);

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

    // A save from before there were 8 LFOs gives those it hasn't the waves they start with.
    for (int i = 0; i < numLfos; ++i)
        setLfoShape (i, waveform::makePreset (startingLfoPresets[(size_t) i]), startingLfoPresets[(size_t) i]);

    // A save from before there were 6 oscillators gives those it hasn't the waves they start with.
    for (int i = 0; i < numOscillators; ++i)
        setShape (i, waveform::makePreset (startingPresets[(size_t) i]), startingPresets[(size_t) i]);

    for (auto& oscillator : oscillators)
    {
        oscillator.tunes = {};
        oscillator.phases = {};
        oscillator.switches = {};
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

            const auto switchTexts = wave["switches"].toString();

            for (int k = 0; k < std::min (switchTexts.length(), waveform::maxHarmonics); ++k)
                oscillators[(size_t) oscillator].switches[(size_t) k] = (waveform::HarmonicSwitch) juce::jlimit (0, 2, (int) (switchTexts[k] - '0'));
        }
    }

    assignments = {};

    // A trigger's threshold, saved since it could be set. Saves from before trigger at the midline, as they did.
    const auto readThreshold = [] (const juce::ValueTree& tree, int target)
    {
        if (! modulation::isTrigger (target))
            return modulation::Assignment().threshold;

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
                                               readThreshold (modulationTree, *target) };
    }

    // Saves from before velocity could be assigned had it make softer notes quieter, as it does to start with.
    velocityAssignments = {};

    if (! velocityTree.isValid())
        velocityAssignments[(size_t) modulation::volumeTarget] = modulation::startingVelocityOnVolume;

    for (const auto& assignmentTree : velocityTree)
        if (const auto target = modulation::findTarget (assignmentTree["target"].toString()))
            velocityAssignments[(size_t) *target] = { modulation::velocitySource,
                                                      juce::jlimit (0.0f, 1.0f, (float) assignmentTree["range"]), false, 0.0f,
                                                      readThreshold (assignmentTree, *target) };

    // Saves from before the mod wheel could be assigned have it move nothing.
    modWheelAssignments = {};

    for (const auto& assignmentTree : modWheelTree)
        if (const auto target = modulation::findTarget (assignmentTree["target"].toString()))
            modWheelAssignments[(size_t) *target] = { modulation::modWheelSource,
                                                      juce::jlimit (0.0f, 1.0f, (float) assignmentTree["range"]),
                                                      (bool) assignmentTree["inverted"], 0.0f, readThreshold (assignmentTree, *target) };

    share();
    copyParametersToSlot();
    ++modulationVersion;
    ++changeCount;
}

//==============================================================================
std::optional<SynthProcessor::KeyRange> SynthProcessor::getPitchSetRange (int index) const
{
    if (index < 0 || index >= getNumPitchSets())
        return std::nullopt;

    return pitchSets[(size_t) index].range;
}

juce::String SynthProcessor::getKeyRangeName (KeyRange range)
{
    const auto name = [] (int note) { return juce::MidiMessage::getMidiNoteName (note, true, true, 4); };

    if (range.lowest == range.highest)
        return name (range.lowest);

    return name (range.lowest) + " to " + name (range.highest);
}

juce::String SynthProcessor::getPitchSetName (int index) const
{
    if (const auto range = getPitchSetRange (index))
        return getKeyRangeName (*range);

    return "Default";
}

void SynthProcessor::activate (int index)
{
    if (index < 0 || index >= getNumPitchSets() || index == activePitchSet)
        return;

    // The sound has only moved from one pitch set to another, so it's no more edited than it was.
    const auto edited = presetEdited.load();
    pitchSets[(size_t) activePitchSet].sound = createSoundState();
    slotOscillators[(size_t) activeSlot.load()] = oscillators;
    activePitchSet = index;
    activeSlot = pitchSets[(size_t) index].slot;
    loadSoundState (pitchSets[(size_t) index].sound.createCopy());
    pitchSets[(size_t) index].sound = {};
    presetEdited = edited;
    ++pitchSetVersion;
}

void SynthProcessor::setActivePitchSet (int index)
{
    activate (index);
}

int SynthProcessor::addPitchSet (KeyRange range)
{
    if (getNumPitchSets() >= maxPitchSets)
        return -1;

    // The first slot no pitch set has.
    auto slot = 0;

    while (std::any_of (pitchSets.begin(), pitchSets.end(), [slot] (const PitchSet& each) { return each.slot == slot; }))
        ++slot;

    const auto from = (size_t) activeSlot.load();
    const auto to = (size_t) slot;

    // It starts as a copy of the active pitch set: its parameters, waves, modulation and LFOs where they are.
    for (size_t i = 0; i < liveValues.size(); ++i)
        slotValues[to][i] = slotValues[from][i].load();

    {
        const juce::SpinLock::ScopedLockType lock (sharedLocks[to]);
        shared[to] = shared[from];
        ++sharedVersions[to];
    }

    lfoPhases[to] = lfoPhases[from];
    lfoCycles[to] = lfoCycles[from];
    slotUsed[to] = true;

    PitchSet pitchSet;
    pitchSet.range = KeyRange { juce::jlimit (0, 127, std::min (range.lowest, range.highest)),
                                juce::jlimit (0, 127, std::max (range.lowest, range.highest)) };
    pitchSet.slot = slot;
    pitchSet.sound = createSoundState();
    slotOscillators[to] = oscillators;
    pitchSets.push_back (pitchSet);
    sortPitchSets();
    updateKeyMap();
    ++pitchSetVersion;
    markEdited();

    for (size_t i = 0; i < pitchSets.size(); ++i)
        if (pitchSets[i].slot == slot)
            return (int) i;

    return -1;
}

int SynthProcessor::setPitchSetRange (int index, KeyRange range)
{
    if (index <= 0 || index >= getNumPitchSets())
        return index;

    const auto slot = pitchSets[(size_t) index].slot;
    pitchSets[(size_t) index].range = KeyRange { juce::jlimit (0, 127, std::min (range.lowest, range.highest)),
                                                 juce::jlimit (0, 127, std::max (range.lowest, range.highest)) };
    sortPitchSets();
    updateKeyMap();
    ++pitchSetVersion;
    markEdited();

    for (size_t i = 0; i < pitchSets.size(); ++i)
        if (pitchSets[i].slot == slot)
            return (int) i;

    return index;
}

void SynthProcessor::removePitchSet (int index)
{
    if (index <= 0 || index >= getNumPitchSets())
        return;

    if (index == activePitchSet)
        activate (0);

    const auto slot = (size_t) pitchSets[(size_t) index].slot;
    pitchSets.erase (pitchSets.begin() + index);

    if (activePitchSet > index)
        --activePitchSet;

    // No key plays its slot any more, before it's let go. Notes already playing it carry on as they were.
    updateKeyMap();
    slotUsed[slot] = false;
    ++pitchSetVersion;
    markEdited();
}

void SynthProcessor::sortPitchSets()
{
    const auto activeSlotNumber = pitchSets[(size_t) activePitchSet].slot;

    std::stable_sort (pitchSets.begin() + 1, pitchSets.end(), [] (const PitchSet& a, const PitchSet& b)
    {
        return a.range->lowest != b.range->lowest ? a.range->lowest < b.range->lowest : a.range->highest < b.range->highest;
    });

    for (size_t i = 0; i < pitchSets.size(); ++i)
        if (pitchSets[i].slot == activeSlotNumber)
            activePitchSet = (int) i;
}

int SynthProcessor::getPitchSetForNote (int note) const
{
    auto best = 0;
    auto bestWidth = 1000;

    for (size_t i = 1; i < pitchSets.size(); ++i)
    {
        const auto& range = *pitchSets[i].range;

        if (range.contains (note) && range.highest - range.lowest < bestWidth)
        {
            best = (int) i;
            bestWidth = range.highest - range.lowest;
        }
    }

    return best;
}

void SynthProcessor::updateKeyMap()
{
    for (int note = 0; note < 128; ++note)
        patches.setForNote[(size_t) note] = pitchSets[(size_t) getPitchSetForNote (note)].slot;
}

void SynthProcessor::loadInitSound()
{
    loadSoundState (initState.createCopy());
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
    loadState (initState.createCopy());
    presetFile = juce::File();
}

void SynthProcessor::nameSound (const juce::String& name)
{
    presetName = name.isNotEmpty() ? name : juce::String (initPresetName);
    presetCategory = otherCategory;
    presetEdited = false;
    factoryPreset = false;
    presetFile = juce::File();
    ++presetVersion;
    ++changeCount;
}

juce::StringArray SynthProcessor::getPresetCategories()
{
    return { "Bass", "Brass and Winds", "Drums", "Keys", "Leads", "Pads", otherCategory };
}

bool SynthProcessor::setPresetCategory (const juce::String& category)
{
    if (factoryPreset || ! getPresetCategories().contains (category))
        return false;

    // A saved preset is moved in its file, leaving the rest of it as it was saved.
    if (presetFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse (presetFile);

        if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
            return false;

        xml->setAttribute ("presetCategory", category);

        if (! xml->writeTo (presetFile))
            return false;
    }

    presetCategory = category;

    // Undoing a change to the sound doesn't move the preset back. Undoing loading it still goes back to the preset
    // before, in its own category.
    for (auto& step : undoHistory)
        if (step.getProperty ("presetName").toString() == presetName && ! (bool) step.getProperty ("presetFactory", false))
            step.setProperty ("presetCategory", category, nullptr);

    ++presetVersion;
    return true;
}

juce::String SynthProcessor::getPresetCategory (const juce::File& file)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return otherCategory;

    juce::MemoryBlock start;
    stream.readIntoMemoryBlock (start, 4096);
    return findCategory (start.toString());
}

juce::String SynthProcessor::getFactoryPresetCategory (const juce::String& name)
{
    for (const auto& preset : getFactoryPresets())
        if (preset.name == name)
            return preset.category;

    return otherCategory;
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

void SynthProcessor::parameterChanged (const juce::String& parameterId, float newValue)
{
    // The active pitch set's slot follows the host's parameters, for the notes it plays.
    if (const auto found = parameterIndices.find (parameterId); found != parameterIndices.end())
        slotValues[(size_t) activeSlot.load()][(size_t) found->second] = newValue;

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
