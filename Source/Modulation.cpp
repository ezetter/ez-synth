#include "Modulation.h"

#include "Tuning.h"

namespace modulation
{
    namespace
    {
        /** What the filters' keys started with before each could be any type. */
        const std::array<const char*, numFilters> oldFilterKeys { "lowPass", "highPass" };
    }

    juce::String getSourceName (int source)
    {
        if (source == velocitySource)
            return "Velocity";

        return isEnvelope (source) ? "Env " + juce::String (source - numLfos + 1) : "LFO " + juce::String (source + 1);
    }

    TargetInfo describe (int target)
    {
        TargetInfo info;

        if (target == volumeTarget)
            info.kind = Kind::volume;
        else if (target >= harmonicPhaseTarget (0, 1))
            info = { Kind::harmonicPhase, (target - harmonicPhaseTarget (0, 1)) / waveform::maxHarmonics,
                     (target - harmonicPhaseTarget (0, 1)) % waveform::maxHarmonics + 1 };
        else if (target >= harmonicTuneTarget (0, 2))
            info = { Kind::harmonicTune, (target - harmonicTuneTarget (0, 2)) / (waveform::maxHarmonics - 1),
                     (target - harmonicTuneTarget (0, 2)) % (waveform::maxHarmonics - 1) + 2 };
        else if (target >= filterDepthTarget (0))
            info = { Kind::filterDepth, 0, 1, target - filterDepthTarget (0) };
        else if (target >= filterQTarget (0))
            info = { Kind::filterQ, 0, 1, target - filterQTarget (0) };
        else if (target >= filterWidthTarget (0))
            info = { Kind::filterWidth, 0, 1, target - filterWidthTarget (0) };
        else if (target >= oscillatorTuneTarget (0))
            info = { Kind::oscillatorTune, target - oscillatorTuneTarget (0) };
        else if (target < oscillatorLevelTarget (0))
            info = { Kind::oscillatorOn, target - oscillatorOnTarget (0) };
        else if (target < oscillatorHarmonicsTarget (0))
            info = { Kind::oscillatorLevel, target - oscillatorLevelTarget (0) };
        else if (target < harmonicTarget (0, 1))
            info = { Kind::oscillatorHarmonics, target - oscillatorHarmonicsTarget (0) };
        else if (target < filterOnTarget (0))
            info = { Kind::harmonic, (target - harmonicTarget (0, 1)) / waveform::maxHarmonics,
                     (target - harmonicTarget (0, 1)) % waveform::maxHarmonics + 1 };
        else if (target < filterCutoffTarget (0))
            info = { Kind::filterOn, 0, 1, target - filterOnTarget (0) };
        else if (target < filterResonanceTarget (0))
            info = { Kind::filterCutoff, 0, 1, target - filterCutoffTarget (0) };
        else
            info = { Kind::filterResonance, 0, 1, target - filterResonanceTarget (0) };

        return info;
    }

    juce::String getName (int target)
    {
        const auto info = describe (target);
        const auto oscillator = "Osc " + juce::String (info.oscillator + 1);
        const auto filter = "Filter " + juce::String (info.filter + 1);

        switch (info.kind)
        {
            case Kind::volume:               return "Volume";
            case Kind::oscillatorOn:         return oscillator + " switch";
            case Kind::oscillatorLevel:      return oscillator + " level";
            case Kind::oscillatorHarmonics:  return oscillator + " number of harmonics";
            case Kind::harmonic:             return oscillator + " harmonic " + juce::String (info.harmonic);
            case Kind::filterOn:             return filter + " switch";
            case Kind::filterCutoff:         return filter + " cutoff";
            case Kind::filterResonance:      return filter + " resonance";
            case Kind::filterWidth:          return filter + " width";
            case Kind::filterQ:              return filter + " Q";
            case Kind::filterDepth:          return filter + " depth";
            case Kind::harmonicTune:         return oscillator + " harmonic " + juce::String (info.harmonic) + " tune";
            case Kind::harmonicPhase:        return oscillator + " harmonic " + juce::String (info.harmonic) + " phase";
            case Kind::oscillatorTune:       return oscillator + " tune";
        }

        return {};
    }

    juce::String getKey (int target)
    {
        const auto info = describe (target);
        const auto oscillator = "osc" + juce::String (info.oscillator + 1);
        const auto filter = "filter" + juce::String (info.filter + 1);

        switch (info.kind)
        {
            case Kind::volume:               return "volume";
            case Kind::oscillatorOn:         return oscillator + "On";
            case Kind::oscillatorLevel:      return oscillator + "Level";
            case Kind::oscillatorHarmonics:  return oscillator + "Harmonics";
            case Kind::harmonic:             return oscillator + "Harmonic" + juce::String (info.harmonic);
            case Kind::filterOn:             return filter + "On";
            case Kind::filterCutoff:         return filter + "Cutoff";
            case Kind::filterResonance:      return filter + "Resonance";
            case Kind::filterWidth:          return filter + "Width";
            case Kind::filterQ:              return filter + "Q";
            case Kind::filterDepth:          return filter + "Depth";
            case Kind::harmonicTune:         return oscillator + "Harmonic" + juce::String (info.harmonic) + "Tune";
            case Kind::harmonicPhase:        return oscillator + "Harmonic" + juce::String (info.harmonic) + "Phase";
            case Kind::oscillatorTune:       return oscillator + "Tune";
        }

        return {};
    }

    juce::String updateKey (const juce::String& key)
    {
        for (size_t filter = 0; filter < oldFilterKeys.size(); ++filter)
            if (key.startsWith (oldFilterKeys[filter]))
                return "filter" + juce::String (filter + 1) + key.substring ((int) std::strlen (oldFilterKeys[filter]));

        return key;
    }

    std::optional<int> findTarget (const juce::String& key)
    {
        const auto updated = updateKey (key);

        for (int target = 0; target < numTargets; ++target)
            if (getKey (target) == updated)
                return target;

        return std::nullopt;
    }

    bool isParameter (int target)
    {
        const auto kind = describe (target).kind;
        return kind != Kind::harmonic && kind != Kind::harmonicTune && kind != Kind::harmonicPhase;
    }

    bool isLoudness (int target)
    {
        const auto kind = describe (target).kind;
        return kind == Kind::volume || kind == Kind::oscillatorOn || kind == Kind::oscillatorLevel;
    }

    const juce::NormalisableRange<float>& amplitudeRange()
    {
        static const auto range = []
        {
            juce::NormalisableRange<float> result (0.0f, waveform::maxAmplitude);
            result.setSkewForCentre (0.35f);
            return result;
        }();

        return range;
    }

    float harmonicsToPosition (int numHarmonics)
    {
        return (float) (numHarmonics - 1) / (float) (waveform::maxHarmonics - 1);
    }

    int positionToHarmonics (float position)
    {
        return 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, position) * (float) (waveform::maxHarmonics - 1));
    }

    float periodToPosition (float seconds)
    {
        return std::log (seconds / shortestPeriod) / std::log (longestPeriod / shortestPeriod);
    }

    float positionToPeriod (float position)
    {
        return shortestPeriod * std::pow (longestPeriod / shortestPeriod, position);
    }

    float getRestPosition (int target)
    {
        const auto kind = describe (target).kind;

        if (kind == Kind::oscillatorTune)
            return tuning::toPosition (0.0f);

        return kind == Kind::harmonicTune ? 0.5f : 0.0f;
    }

    float applyEnvelope (int target, const Assignment& assignment, float position, const EnvelopeState& envelope)
    {
        if (! envelope.releasing)
            return juce::jlimit (0.0f, 1.0f, position + assignment.offset (envelope.level));

        const auto rest = getRestPosition (target);
        const auto atKeyUp = juce::jlimit (0.0f, 1.0f, position + assignment.offset (envelope.releaseFrom));
        return rest + (atKeyUp - rest) * juce::jlimit (0.0f, 1.0f, envelope.releaseLeft);
    }

    juce::String phaseToText (float phase)
    {
        return juce::String (juce::roundToInt (phase * 360.0f)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
    }

    float lfoValue (const waveform::Shape& shape, double periodsGone, bool once)
    {
        // An LFO that runs once, shifted to start later, waits at its first value until it does.
        if (once && periodsGone <= 0.0)
            return shape.front();

        if (once && periodsGone >= 1.0)
            return shape.back();

        return waveform::valueAt (shape, periodsGone - std::floor (periodsGone), ! once);
    }
}
