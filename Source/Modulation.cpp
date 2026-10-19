#include "Modulation.h"

namespace modulation
{
    namespace
    {
        const std::array<const char*, numFilters> filterKeys { "lowPass", "highPass" };
        const std::array<const char*, numFilters> filterNames { "Low pass", "High pass" };
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
        const juce::String filter (filterNames[(size_t) info.filter]);

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
        }

        return {};
    }

    juce::String getKey (int target)
    {
        const auto info = describe (target);
        const auto oscillator = "osc" + juce::String (info.oscillator + 1);
        const juce::String filter (filterKeys[(size_t) info.filter]);

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
        }

        return {};
    }

    std::optional<int> findTarget (const juce::String& key)
    {
        for (int target = 0; target < numTargets; ++target)
            if (getKey (target) == key)
                return target;

        return std::nullopt;
    }

    bool isParameter (int target)
    {
        return describe (target).kind != Kind::harmonic;
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

    float lfoValue (const waveform::Shape& shape, double periodsGone, bool once)
    {
        if (once && periodsGone >= 1.0)
            return shape.back();

        return waveform::valueAt (shape, periodsGone - std::floor (periodsGone), ! once);
    }
}
