#include "SoundDescription.h"

#include "Tuning.h"

namespace sound
{
    namespace
    {
        constexpr auto degreesPerRadian = 180.0 / juce::MathConstants<double>::pi;

        /** A number cut to a few significant figures, so a description is easy to read. */
        double tidy (double value, int figures = 4)
        {
            if (juce::exactlyEqual (value, 0.0) || ! std::isfinite (value))
                return 0.0;

            const auto scale = std::pow (10.0, figures - 1 - (int) std::floor (std::log10 (std::abs (value))));
            return std::round (value * scale) / scale;
        }

        juce::RangedAudioParameter* findParameter (SynthProcessor& synth, const juce::String& id)
        {
            const auto updated = modulation::updateKey (id);

            for (auto* parameter : synth.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    if (ranged->paramID == updated || ranged->paramID.equalsIgnoreCase (updated))
                        return ranged;

            return nullptr;
        }

        /** A parameter's value in its own units: a number, true or false for a switch, or a choice's name. */
        juce::var parameterValue (const juce::RangedAudioParameter& parameter, float normalised)
        {
            const auto value = parameter.convertFrom0to1 (normalised);

            if (dynamic_cast<const juce::AudioParameterBool*> (&parameter) != nullptr)
                return value >= 0.5f;

            if (const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (&parameter))
                return choice->choices[juce::roundToInt (value)];

            return tidy (value, 5);
        }

        /** Sets a parameter from a value in its own units. Returns a problem, if there is one. */
        juce::String setParameter (juce::RangedAudioParameter& parameter, const juce::var& value)
        {
            float plain = 0.0f;

            if (const auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&parameter); choice != nullptr && value.isString())
            {
                const auto index = choice->choices.indexOf (value.toString(), true);

                if (index < 0)
                    return parameter.paramID + ": \"" + value.toString() + "\" isn't one of its choices ("
                         + choice->choices.joinIntoString (", ") + ")";

                plain = (float) index;
            }
            else if (value.isBool())
            {
                plain = (bool) value ? 1.0f : 0.0f;
            }
            else if (value.isDouble() || value.isInt() || value.isInt64())
            {
                plain = (float) (double) value;

                if (! std::isfinite (plain))
                    return parameter.paramID + ": not a number";
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (&parameter) != nullptr && value.isString())
            {
                plain = value.toString().equalsIgnoreCase ("true") || value.toString().equalsIgnoreCase ("on") ? 1.0f : 0.0f;
            }
            else
            {
                return parameter.paramID + ": its value should be a number" + (dynamic_cast<juce::AudioParameterChoice*> (&parameter) != nullptr
                                                                                   ? juce::String (" or one of its choices") : juce::String());
            }

            const auto& range = parameter.getNormalisableRange();
            parameter.setValueNotifyingHost (parameter.convertTo0to1 (juce::jlimit (range.start, range.end, plain)));
            return {};
        }

        std::optional<int> findSource (const juce::String& name)
        {
            for (int source = 0; source < modulation::numSources; ++source)
                if (modulation::getSourceName (source).equalsIgnoreCase (name.trim())
                     || modulation::getSourceName (source).removeCharacters (" ").equalsIgnoreCase (name.removeCharacters (" ")))
                    return source;

            // "Envelope 2" as well as "Env 2".
            if (name.startsWithIgnoreCase ("envelope"))
                return findSource ("Env " + name.fromFirstOccurrenceOf ("envelope", false, true).trim());

            return std::nullopt;
        }

        juce::String getSwitchName (waveform::HarmonicSwitch value)
        {
            switch (value)
            {
                case waveform::HarmonicSwitch::off:      return "off";
                case waveform::HarmonicSwitch::on:       return "on";
                case waveform::HarmonicSwitch::follows:  break;
            }

            return "follows";
        }

        std::optional<waveform::HarmonicSwitch> findSwitch (const juce::String& name)
        {
            for (const auto value : { waveform::HarmonicSwitch::follows, waveform::HarmonicSwitch::off, waveform::HarmonicSwitch::on })
                if (getSwitchName (value).equalsIgnoreCase (name.trim()))
                    return value;

            return std::nullopt;
        }

        std::optional<double> number (const juce::var& object, const juce::Identifier& name)
        {
            if (const auto& value = object[name]; value.isDouble() || value.isInt() || value.isInt64())
                if (std::isfinite ((double) value))
                    return (double) value;

            return std::nullopt;
        }

        /** A wave from points evenly spaced across one period, the first at its start, drawn in straight lines from
            each to the next and from the last back to the first, or held in steps, one point to a step.
        */
        waveform::Shape shapeFromPoints (const juce::Array<juce::var>& points, bool stepped, float limit)
        {
            waveform::Shape shape {};
            const auto count = points.size();

            for (int i = 0; i < waveform::numPoints; ++i)
            {
                const auto position = (double) i / waveform::numPoints * count;
                const auto first = juce::jlimit (0, count - 1, (int) std::floor (position));
                const auto value = [&points] (int index) { return (float) (double) points[index]; };

                shape[(size_t) i] = stepped ? value (first)
                                            : value (first) + (value ((first + 1) % count) - value (first)) * (float) (position - first);
                shape[(size_t) i] = std::isfinite (shape[(size_t) i]) ? juce::jlimit (-limit, limit, shape[(size_t) i]) : 0.0f;
            }

            return shape;
        }

        void loadOscillator (SynthProcessor& synth, int oscillator, const juce::var& description, bool setsHarmonicCount,
                             juce::StringArray& problems)
        {
            const auto name = "oscillators[" + juce::String (oscillator) + "]";

            if (! description.isObject())
            {
                problems.add (name + " should be an object");
                return;
            }

            // A preset to start from, or nothing, so the harmonics listed are all it has.
            std::optional<waveform::Preset> preset;

            if (const auto wave = description["wave"]; wave.isString())
            {
                preset = waveform::findPreset (wave.toString());

                if (! preset.has_value())
                    for (const auto each : waveform::allPresets)
                        if (waveform::getName (each).equalsIgnoreCase (wave.toString()))
                            preset = each;

                if (! preset.has_value())
                    problems.add (name + ".wave: \"" + wave.toString() + "\" isn't Sine, Sawtooth, Square or Triangle");
            }

            if (const auto points = description["points"]; points.isArray() && points.getArray()->size() >= 2)
                synth.setShape (oscillator, shapeFromPoints (*points.getArray(), (bool) description["stepped"], waveform::maxAmplitude));
            else
                synth.setShape (oscillator, preset.has_value() ? waveform::makePreset (*preset) : waveform::Shape {}, preset);

            for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
            {
                synth.setHarmonicTune (oscillator, harmonic, 0.0f);
                synth.setHarmonicSwitch (oscillator, harmonic, waveform::HarmonicSwitch::follows);
            }

            auto highest = 0;

            if (const auto harmonics = description["harmonics"]; harmonics.isArray())
            {
                for (const auto& entry : *harmonics.getArray())
                {
                    const auto harmonic = juce::roundToInt (number (entry, "harmonic").value_or (0.0));

                    if (harmonic < 1 || harmonic > waveform::maxHarmonics)
                    {
                        problems.add (name + ": harmonic " + juce::String (harmonic) + " isn't between 1 and "
                                      + juce::String (waveform::maxHarmonics));
                        continue;
                    }

                    // The phase first, so a harmonic raised from nothing comes in at it. A phase the harmonic already
                    // has, to the tenth of a degree it's written to, is left alone.
                    auto phaseChanged = false;

                    if (const auto phase = number (entry, "phase"))
                    {
                        const auto turn = juce::MathConstants<float>::twoPi;
                        const auto wanted = (float) (*phase / degreesPerRadian);
                        const auto apart = std::abs (std::remainder (wanted - synth.getHarmonicPhase (oscillator, harmonic), turn));
                        phaseChanged = apart > 0.002f;

                        if (phaseChanged)
                            synth.setHarmonicPhase (oscillator, harmonic, wanted);
                    }

                    if (const auto amplitude = number (entry, "amplitude"))
                    {
                        const auto clamped = juce::jlimit (0.0f, waveform::maxAmplitude, (float) *amplitude);
                        const auto current = synth.getDecomposition (oscillator).harmonics[(size_t) harmonic - 1].amplitude;

                        // A preset's own harmonics, given again, leave it the preset.
                        if (std::abs (clamped - current) > 1.0e-3f || phaseChanged)
                            synth.setHarmonicAmplitude (oscillator, harmonic, clamped);
                    }

                    if (const auto tune = number (entry, "tune"))
                        synth.setHarmonicTune (oscillator, harmonic, (float) *tune);

                    if (const auto switchName = entry["switch"]; switchName.isString())
                    {
                        if (const auto value = findSwitch (switchName.toString()))
                            synth.setHarmonicSwitch (oscillator, harmonic, *value);
                        else
                            problems.add (name + ": harmonic " + juce::String (harmonic) + "'s switch should be follows, off or on");
                    }
                }
            }

            for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
                if (synth.getDecomposition (oscillator).harmonics[(size_t) harmonic - 1].amplitude > 1.0e-4f)
                    highest = harmonic;

            // Unless the parameters say how many harmonics it plays, it plays as many as it's given.
            if (setsHarmonicCount && highest > 0 && description["harmonics"].isArray())
                if (auto* count = findParameter (synth, SynthProcessor::harmonicsId (oscillator)))
                    setParameter (*count, highest);

            if (description.hasProperty ("stepped") || description.hasProperty ("steps"))
                synth.setDrawing (oscillator, { (bool) description["stepped"],
                                                juce::jlimit (waveform::fewestSteps, waveform::mostSteps,
                                                              juce::roundToInt (number (description, "steps").value_or (8.0))) });
        }

        void loadLfo (SynthProcessor& synth, int lfo, const juce::var& description, juce::StringArray& problems)
        {
            const auto name = "lfos[" + juce::String (lfo) + "]";

            if (! description.isObject())
            {
                problems.add (name + " should be an object");
                return;
            }

            const auto stepped = (bool) description["stepped"];
            const auto points = description["points"];

            if (points.isArray() && points.getArray()->size() >= 2)
            {
                synth.setLfoShape (lfo, shapeFromPoints (*points.getArray(), stepped, 1.0f));
            }
            else if (const auto wave = description["wave"]; wave.isString())
            {
                std::optional<waveform::Preset> preset;

                for (const auto each : waveform::allPresets)
                    if (waveform::getName (each).equalsIgnoreCase (wave.toString()))
                        preset = each;

                if (preset.has_value())
                    synth.setLfoShape (lfo, waveform::makePreset (*preset), preset);
                else
                    problems.add (name + ".wave: \"" + wave.toString() + "\" isn't Sine, Sawtooth, Square or Triangle");
            }

            const auto steps = juce::roundToInt (number (description, "steps").value_or (points.isArray() && stepped ? points.getArray()->size() : 8));
            synth.setDrawing (SynthProcessor::numOscillators + lfo,
                              { stepped, juce::jlimit (waveform::fewestSteps, waveform::mostSteps, steps) });
        }

        void loadAssignments (SynthProcessor& synth, const juce::var& list, const juce::String& listName, int kind,
                              juce::StringArray& problems)
        {
            if (! list.isArray())
            {
                problems.add (listName + " should be a list");
                return;
            }

            for (const auto& entry : *list.getArray())
            {
                const auto key = entry["target"].toString();
                const auto target = modulation::findTarget (key);

                if (! target.has_value())
                {
                    problems.add (listName + ": there's no control \"" + key + "\" to move");
                    continue;
                }

                modulation::Assignment assignment;
                assignment.range = juce::jlimit (0.0f, 1.0f, (float) number (entry, "range").value_or (0.5));
                assignment.inverted = (bool) entry["inverted"];
                assignment.threshold = juce::jlimit (0.0f, 1.0f, (float) number (entry, "threshold").value_or (0.5));

                if (kind == 0)
                {
                    const auto source = findSource (entry["source"].toString());

                    if (! source.has_value())
                    {
                        problems.add (listName + ": \"" + entry["source"].toString() + "\" isn't an LFO or envelope, such as LFO 1 or Env 2");
                        continue;
                    }

                    if (synth.wouldMakeLoop (*target, *source))
                    {
                        problems.add (listName + ": " + modulation::getSourceName (*source) + " can't move " + key
                                      + ", as an LFO can't move its own rate, even through others");
                        continue;
                    }

                    assignment.source = *source;
                    assignment.phase = modulation::isEnvelope (*source) ? 0.0f
                                                                        : (float) (number (entry, "phase").value_or (0.0) / 360.0);
                    assignment.phase -= std::floor (assignment.phase);
                    synth.setAssignment (*target, assignment);
                }
                else if (kind == 1)
                {
                    assignment.source = modulation::velocitySource;
                    assignment.inverted = false;
                    synth.setVelocityAssignment (*target, assignment);
                }
                else
                {
                    assignment.source = modulation::modWheelSource;
                    synth.setModWheelAssignment (*target, assignment);
                }
            }
        }
    }

    namespace
    {
        /** Loads a sound's parameters, oscillators, LFOs and modulation into the active pitch set, on top of what it
            has, which is Init's.
        */
        void loadSound (SynthProcessor& synth, const juce::var& description, int version, juce::StringArray& problems)
        {
            // The parameters, in their own units. How many harmonics an oscillator plays is set from its harmonics,
            // unless it's given here.
            const auto parameters = description["parameters"];

            if (parameters.isObject())
            {
                // A sound that glides its oscillators each as it's set, without saying whether they glide alike,
                // glides them each as it's set.
                auto ownGlide = false;

                for (const auto& property : parameters.getDynamicObject()->getProperties())
                    ownGlide = ownGlide || property.name.toString().matchesWildcard ("osc*Glide*", true);

                if (ownGlide && ! parameters.getDynamicObject()->hasProperty (SynthProcessor::glideAllId))
                    if (auto* all = findParameter (synth, SynthProcessor::glideAllId))
                        all->setValueNotifyingHost (0.0f);

                for (const auto& property : parameters.getDynamicObject()->getProperties())
                {
                    if (auto* parameter = findParameter (synth, property.name.toString()))
                    {
                        if (const auto problem = setParameter (*parameter, property.value); problem.isNotEmpty())
                            problems.add ("parameters." + problem);
                    }
                    else
                    {
                        problems.add ("parameters: there's no parameter \"" + property.name.toString() + "\"");
                    }
                }

            }
            else if (! parameters.isVoid())
            {
                problems.add ("parameters should be an object, of parameter IDs and values");
            }

            // Version 1 was written when the tune slider reached an octave either side, zoomed out, so its tunes' zooms,
            // given or left as Init's, are moved to where they reach as far as they did, and modulation moves the tunes
            // as far as it did.
            if (version < 2)
                for (int i = 0; i < SynthProcessor::numOscillators; ++i)
                    if (auto* zoom = synth.parameters.getParameter (SynthProcessor::tuneZoomId (i)))
                        zoom->setValueNotifyingHost (tuning::updateOldZoom (zoom->getValue()));

            if (const auto oscillators = description["oscillators"]; oscillators.isArray())
            {
                for (int i = 0; i < std::min (oscillators.getArray()->size(), SynthProcessor::numOscillators); ++i)
                {
                    const auto& entry = (*oscillators.getArray())[i];

                    if (entry.isObject())
                        loadOscillator (synth, i, entry, ! (parameters.isObject() && parameters.hasProperty (SynthProcessor::harmonicsId (i))),
                                        problems);
                }

                if (oscillators.getArray()->size() > SynthProcessor::numOscillators)
                    problems.add ("oscillators: there are only " + juce::String (SynthProcessor::numOscillators) + ", so the rest were left out");
            }

            if (const auto lfos = description["lfos"]; lfos.isArray())
            {
                for (int i = 0; i < std::min (lfos.getArray()->size(), SynthProcessor::numLfos); ++i)
                    if (const auto& entry = (*lfos.getArray())[i]; entry.isObject())
                        loadLfo (synth, i, entry, problems);

                if (lfos.getArray()->size() > SynthProcessor::numLfos)
                    problems.add ("lfos: there are only " + juce::String (SynthProcessor::numLfos) + ", so the rest were left out");
            }

            if (const auto modulations = description["modulation"]; ! modulations.isVoid())
                loadAssignments (synth, modulations, "modulation", 0, problems);

            // Listed, velocity moves only what's listed, even nothing; left out, it stays on the volume, as Init has it.
            if (const auto velocity = description["velocity"]; ! velocity.isVoid())
            {
                for (int target = 0; target < modulation::numTargets; ++target)
                    synth.setVelocityAssignment (target, {});

                loadAssignments (synth, velocity, "velocity", 1, problems);
            }

            if (const auto modWheel = description["modWheel"]; ! modWheel.isVoid())
                loadAssignments (synth, modWheel, "modWheel", 2, problems);
        }
    }

    std::optional<int> findNote (const juce::var& value)
    {
        if (value.isInt() || value.isInt64() || value.isDouble())
        {
            const auto note = juce::roundToInt ((double) value);
            return note >= 0 && note <= 127 ? std::optional (note) : std::nullopt;
        }

        const auto text = value.toString().trim().toUpperCase();

        if (text.isEmpty())
            return std::nullopt;

        static constexpr int letters[] = { 9, 11, 0, 2, 4, 5, 7 };   // A to G

        if (text[0] < 'A' || text[0] > 'G')
            return std::nullopt;

        auto pitch = letters[text[0] - 'A'];
        auto rest = text.substring (1);

        while (rest.startsWithChar ('#') || rest.startsWithChar ('B') || rest.startsWith (juce::CharPointer_UTF8 ("\xe2\x99\xaf"))
               || rest.startsWith (juce::CharPointer_UTF8 ("\xe2\x99\xad")))
        {
            const auto sharp = rest.startsWithChar ('#') || rest.startsWith (juce::CharPointer_UTF8 ("\xe2\x99\xaf"));
            pitch += sharp ? 1 : -1;
            rest = rest.substring (1);
        }

        if (! rest.containsOnly ("-0123456789") || rest.isEmpty())
            return std::nullopt;

        const auto note = (rest.getIntValue() + 1) * 12 + pitch;
        return note >= 0 && note <= 127 ? std::optional (note) : std::nullopt;
    }

    LoadResult load (SynthProcessor& synth, const juce::String& json)
    {
        LoadResult result;
        juce::var description;

        // Claude may wrap the JSON in a Markdown code block, or say something before or after it.
        auto text = json.trim();

        if (! text.startsWith ("{") && text.contains ("{"))
            text = text.fromFirstOccurrenceOf ("{", true, false).upToLastOccurrenceOf ("}", true, false);

        if (const auto parsed = juce::JSON::parse (text, description); parsed.failed() || ! description.isObject())
        {
            result.problems.add (parsed.failed() ? "It isn't JSON: " + parsed.getErrorMessage()
                                                 : juce::String ("It should be a JSON object, in {}"));
            return result;
        }

        if (description.hasProperty ("format") && description["format"].toString() != formatName)
            result.problems.add ("Its format is \"" + description["format"].toString() + "\" rather than \"" + formatName
                                 + "\", but it's been loaded as one anyway");

        synth.loadInitPreset();
        const auto version = (int) description.getProperty ("version", formatVersion);
        loadSound (synth, description, version, result.problems);

        // Each pitch set is a sound of its own for a range of keys, starting from Init, as the default does.
        if (const auto list = description["pitchSets"]; list.isArray())
        {
            for (int i = 0; i < list.getArray()->size(); ++i)
            {
                const auto& entry = (*list.getArray())[i];
                const auto name = "pitchSets[" + juce::String (i) + "]";
                const auto lowest = findNote (entry["lowest"]);
                const auto highest = findNote (entry["highest"]);

                if (! entry.isObject() || ! lowest.has_value() || ! highest.has_value())
                {
                    result.problems.add (name + " should have a lowest and a highest key, as note names such as \"C2\" or MIDI note numbers, so it was left out");
                    continue;
                }

                const auto index = synth.addPitchSet ({ *lowest, *highest });

                if (index < 0)
                {
                    result.problems.add (name + ": there can only be " + juce::String (SynthProcessor::maxPitchSets)
                                         + " pitch sets, the default among them, so the rest were left out");
                    break;
                }

                synth.setActivePitchSet (index);
                synth.loadInitSound();

                juce::StringArray problems;
                loadSound (synth, entry, version, problems);

                for (const auto& problem : problems)
                    result.problems.add (name + "." + problem);
            }

            synth.setActivePitchSet (0);
        }
        else if (! list.isVoid())
        {
            result.problems.add ("pitchSets should be a list");
        }

        synth.nameSound (description["name"].toString().trim().isNotEmpty() ? description["name"].toString().trim()
                                                                             : juce::String ("From Claude"));
        result.loaded = true;
        return result;
    }

    LoadResult loadFile (SynthProcessor& synth, const juce::File& file)
    {
        if (! file.existsAsFile())
            return { false, { "There's no file " + file.getFullPathName() } };

        auto result = load (synth, file.loadFileAsString());

        // A description with no name is named after its file.
        if (result.loaded && synth.getPresetName() == "From Claude")
            synth.nameSound (file.getFileNameWithoutExtension());

        return result;
    }

    namespace
    {
        /** Describes the active pitch set's sound into an object: its parameters, oscillators, LFOs and modulation. */
        void describeSound (juce::DynamicObject* root, const SynthProcessor& synth, bool onlyChanged)
        {
            auto* parameterObject = new juce::DynamicObject();

            for (auto* parameter : synth.getParameters())
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                {
                    const auto value = ranged->getValue();

                    // How many harmonics each oscillator plays is always given, as loading harmonics without it plays
                    // as many as there are.
                    const auto isHarmonicCount = ranged->paramID.startsWith ("osc") && ranged->paramID.endsWith ("Harmonics");

                    if (! onlyChanged || isHarmonicCount || std::abs (value - ranged->getDefaultValue()) > 1.0e-4f)
                        parameterObject->setProperty (ranged->paramID, parameterValue (*ranged, value));
                }
            }

            root->setProperty ("parameters", juce::var (parameterObject));

            juce::Array<juce::var> oscillators;

            for (int i = 0; i < SynthProcessor::numOscillators; ++i)
            {
                auto* oscillator = new juce::DynamicObject();

                if (const auto preset = synth.getPreset (i))
                    oscillator->setProperty ("wave", waveform::getName (*preset));

                juce::Array<juce::var> harmonics;

                for (int harmonic = 1; harmonic <= waveform::maxHarmonics; ++harmonic)
                {
                    const auto& partial = synth.getDecomposition (i).harmonics[(size_t) harmonic - 1];
                    const auto tune = synth.getHarmonicTune (i, harmonic);
                    const auto switched = synth.getHarmonicSwitch (i, harmonic);

                    if (partial.amplitude < 5.0e-4f && juce::exactlyEqual (tune, 0.0f) && switched == waveform::HarmonicSwitch::follows)
                        continue;

                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("harmonic", harmonic);
                    entry->setProperty ("amplitude", std::round (partial.amplitude * 10000.0f) / 10000.0);

                    auto phase = (double) synth.getHarmonicPhase (i, harmonic) * degreesPerRadian;
                    phase -= 360.0 * std::floor (phase / 360.0);

                    if (const auto rounded = std::round (phase * 10.0) / 10.0; rounded > 0.0 && rounded < 360.0)
                        entry->setProperty ("phase", rounded);

                    if (! juce::exactlyEqual (tune, 0.0f))
                        entry->setProperty ("tune", tidy (tune));

                    if (switched != waveform::HarmonicSwitch::follows)
                        entry->setProperty ("switch", getSwitchName (switched));

                    harmonics.add (juce::var (entry));
                }

                oscillator->setProperty ("harmonics", harmonics);

                if (const auto& drawing = synth.getDrawing (i); drawing.stepped)
                {
                    oscillator->setProperty ("stepped", true);
                    oscillator->setProperty ("steps", drawing.numSteps);
                }

                oscillators.add (juce::var (oscillator));
            }

            root->setProperty ("oscillators", oscillators);

            // The LFOs that move something, or are on, or all of them.
            juce::Array<juce::var> lfos;
            auto lastShown = -1;

            for (int i = 0; i < SynthProcessor::numLfos; ++i)
            {
                auto used = synth.isLfoOn (i);

                for (int target = 0; target < modulation::numTargets && ! used; ++target)
                    used = synth.getAssignment (target).source == i;

                if (! onlyChanged || used)
                    lastShown = i;
            }

            for (int i = 0; i <= lastShown; ++i)
            {
                auto* lfo = new juce::DynamicObject();
                const auto& drawing = synth.getDrawing (SynthProcessor::numOscillators + i);

                if (const auto preset = synth.getLfoPreset (i))
                {
                    lfo->setProperty ("wave", waveform::getName (*preset));
                }
                else
                {
                    // A stepped wave as its steps' levels, and a smooth one as 128 points along it, which is close enough
                    // to hear no difference for most waves. One with detail 128 points would lose, such as a quick
                    // attack drawn on it, is written as every point it's drawn with, to five places.
                    const auto& shape = synth.getLfoShape (i);
                    const auto pointsOf = [&shape, &drawing] (int count, float scale)
                    {
                        std::vector<double> values;

                        for (int point = 0; point < count; ++point)
                        {
                            const auto position = drawing.stepped ? ((double) point + 0.5) / count : (double) point / count;
                            values.push_back (std::round (waveform::valueAt (shape, position) * scale) / (double) scale);
                        }

                        return values;
                    };

                    auto values = pointsOf (drawing.stepped ? drawing.numSteps : 128, 1000.0f);

                    if (! drawing.stepped)
                    {
                        // The 128 points joined by straight lines, as they're loaded, against the wave drawn.
                        auto furthest = 0.0;

                        for (int k = 0; k < waveform::numPoints; ++k)
                        {
                            const auto along = (double) k * (double) values.size() / waveform::numPoints;
                            const auto before = (size_t) along;
                            const auto proportion = along - (double) before;
                            const auto joined = values[before] + proportion * (values[(before + 1) % values.size()] - values[before]);
                            furthest = std::max (furthest, std::abs (joined - (double) shape[(size_t) k]));
                        }

                        if (furthest > 0.002)
                            values = pointsOf (waveform::numPoints, 100000.0f);
                    }

                    juce::Array<juce::var> points;

                    for (const auto value : values)
                        points.add (value);

                    lfo->setProperty ("points", points);
                }

                if (drawing.stepped)
                {
                    lfo->setProperty ("stepped", true);
                    lfo->setProperty ("steps", drawing.numSteps);
                }

                lfos.add (juce::var (lfo));
            }

            root->setProperty ("lfos", lfos);

            juce::Array<juce::var> modulations, velocity, modWheel;

            for (int target = 0; target < modulation::numTargets; ++target)
            {
                const auto key = modulation::getKey (target);
                const auto trigger = modulation::isTrigger (target);

                if (const auto& assignment = synth.getAssignment (target); assignment.isActive())
                {
                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("target", key);
                    entry->setProperty ("source", modulation::getSourceName (assignment.source));
                    entry->setProperty ("range", tidy (assignment.range));

                    if (assignment.inverted)
                        entry->setProperty ("inverted", true);

                    if (! modulation::isEnvelope (assignment.source) && assignment.phase > 0.0f)
                        entry->setProperty ("phase", std::round (assignment.phase * 3600.0f) / 10.0);

                    if (trigger)
                        entry->setProperty ("threshold", tidy (assignment.threshold));

                    modulations.add (juce::var (entry));
                }

                if (const auto& assignment = synth.getVelocityAssignment (target); assignment.isActive())
                {
                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("target", key);
                    entry->setProperty ("range", tidy (assignment.range));

                    if (trigger)
                        entry->setProperty ("threshold", tidy (assignment.threshold));

                    velocity.add (juce::var (entry));
                }

                if (const auto& assignment = synth.getModWheelAssignment (target); assignment.isActive())
                {
                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("target", key);
                    entry->setProperty ("range", tidy (assignment.range));

                    if (assignment.inverted)
                        entry->setProperty ("inverted", true);

                    if (trigger)
                        entry->setProperty ("threshold", tidy (assignment.threshold));

                    modWheel.add (juce::var (entry));
                }
            }

            root->setProperty ("modulation", modulations);
            root->setProperty ("velocity", velocity);

            if (! onlyChanged || ! modWheel.isEmpty())
                root->setProperty ("modWheel", modWheel);
        }
    }

    juce::var describe (SynthProcessor& synth, bool onlyChanged)
    {
        auto* root = new juce::DynamicObject();
        juce::var description (root);
        root->setProperty ("format", formatName);
        root->setProperty ("version", formatVersion);
        root->setProperty ("name", synth.getPresetName());

        // The default pitch set's sound, then each other's, for its range of keys, going back to the one active.
        const auto active = synth.getActivePitchSet();
        synth.setActivePitchSet (0);
        describeSound (root, synth, onlyChanged);

        if (synth.getNumPitchSets() > 1)
        {
            juce::Array<juce::var> pitchSets;

            for (int i = 1; i < synth.getNumPitchSets(); ++i)
            {
                synth.setActivePitchSet (i);
                auto* pitchSet = new juce::DynamicObject();
                const auto range = *synth.getPitchSetRange (i);
                pitchSet->setProperty ("lowest", juce::MidiMessage::getMidiNoteName (range.lowest, true, true, 4));
                pitchSet->setProperty ("highest", juce::MidiMessage::getMidiNoteName (range.highest, true, true, 4));
                describeSound (pitchSet, synth, onlyChanged);
                pitchSets.add (juce::var (pitchSet));
            }

            root->setProperty ("pitchSets", pitchSets);
        }

        synth.setActivePitchSet (active);
        return description;
    }

    namespace
    {
        bool isPlain (const juce::var& value)
        {
            return ! value.isObject() && ! value.isArray();
        }

        /** Lays JSON out to read easily: small objects of plain values, such as a harmonic, and lists of plain values,
            such as an LFO's points, on one line each, and everything else a line to each entry.
        */
        void write (juce::String& text, const juce::var& value, int indent)
        {
            const auto inline_ = juce::JSON::FormatOptions().withSpacing (juce::JSON::Spacing::singleLine).withMaxDecimalPlaces (5);
            const auto margin = juce::String::repeatedString (" ", indent + 2);

            if (const auto* object = value.getDynamicObject())
            {
                const auto& properties = object->getProperties();
                auto allPlain = true;

                for (const auto& property : properties)
                    allPlain = allPlain && isPlain (property.value);

                if (allPlain && properties.size() <= 6)
                {
                    text << juce::JSON::toString (value, inline_);
                    return;
                }

                text << "{\n";

                for (int i = 0; i < properties.size(); ++i)
                {
                    text << margin << juce::JSON::toString (properties.getName (i).toString(), inline_) << ": ";
                    write (text, properties.getValueAt (i), indent + 2);
                    text << (i + 1 < properties.size() ? ",\n" : "\n");
                }

                text << juce::String::repeatedString (" ", indent) << "}";
                return;
            }

            if (const auto* array = value.getArray(); array != nullptr && ! array->isEmpty())
            {
                if (std::all_of (array->begin(), array->end(), isPlain))
                {
                    text << juce::JSON::toString (value, inline_);
                    return;
                }

                text << "[\n";

                for (int i = 0; i < array->size(); ++i)
                {
                    text << margin;
                    write (text, (*array)[i], indent + 2);
                    text << (i + 1 < array->size() ? ",\n" : "\n");
                }

                text << juce::String::repeatedString (" ", indent) << "]";
                return;
            }

            text << juce::JSON::toString (value, inline_);
        }
    }

    juce::String toJson (SynthProcessor& synth, bool onlyChanged)
    {
        juce::String text;
        write (text, describe (synth, onlyChanged), 0);
        return text;
    }

    juce::File getClaudeFolder()
    {
        return SynthProcessor::getPresetFolder().getParentDirectory().getChildFile ("From Claude");
    }

    juce::Array<juce::File> findClaudeSounds (const juce::File& folder)
    {
        auto sounds = folder.findChildFiles (juce::File::findFiles, false, "*.json");

        std::sort (sounds.begin(), sounds.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getLastModificationTime() > b.getLastModificationTime();
        });

        return sounds;
    }
}
