#include "FilterStage.h"

juce::String FilterStage::getName (FilterType type)
{
    switch (type)
    {
        case FilterType::lowPass:   return "Low pass";
        case FilterType::midPass:   return "Mid pass";
        case FilterType::highPass:  return "High pass";
    }

    return {};
}

juce::String FilterStage::getShortName (FilterType type)
{
    switch (type)
    {
        case FilterType::lowPass:   return "Low";
        case FilterType::midPass:   return "Mid";
        case FilterType::highPass:  return "High";
    }

    return {};
}

float FilterStage::resonanceToQ (float resonance)
{
    return juce::MathConstants<float>::sqrt2 / 2.0f * std::pow (2.0f, 3.5f * juce::jlimit (0.0f, 1.0f, resonance));
}

const juce::NormalisableRange<float>& FilterStage::widthRange()
{
    static const auto range = []
    {
        juce::NormalisableRange<float> result (0.2f, 6.0f);
        result.setSkewForCentre (1.5f);
        return result;
    }();

    return range;
}

float FilterStage::widthToQ (float octaves)
{
    const auto ratio = std::pow (2.0f, juce::jlimit (widthRange().start, widthRange().end, octaves));
    return std::sqrt (ratio) / (ratio - 1.0f);
}

float FilterStage::cutoffToPosition (float cutoff)
{
    return std::log2 (juce::jlimit (lowestCutoff, highestCutoff, cutoff) / lowestCutoff)
         / std::log2 (highestCutoff / lowestCutoff);
}

float FilterStage::positionToCutoff (float position)
{
    return lowestCutoff * std::pow (highestCutoff / lowestCutoff, juce::jlimit (0.0f, 1.0f, position));
}

float FilterStage::limitCutoff (float newCutoff) const
{
    return juce::jlimit (lowestCutoff, std::min (highestCutoff, (float) (0.45 * sampleRate)), newCutoff);
}

float FilterStage::getQ (const Settings& settings) const
{
    return settings.type == FilterType::midPass ? widthToQ (settings.width) : resonanceToQ (settings.resonance);
}

void FilterStage::prepare (double newSampleRate, const Settings& settings)
{
    sampleRate = newSampleRate;

    // Quick enough to follow an LFO, slow enough not to click.
    cutoff.reset (sampleRate, 0.01);
    q.reset (sampleRate, 0.01);
    mix.reset (sampleRate, 0.02);
    typeMix.reset (sampleRate, 0.02);

    jumpTo (settings);
}

void FilterStage::jumpTo (const Settings& settings)
{
    cutoff.setCurrentAndTargetValue (limitCutoff (settings.cutoff));
    q.setCurrentAndTargetValue (getQ (settings));
    mix.setCurrentAndTargetValue (settings.on ? 1.0f : 0.0f);
    typeMix.setCurrentAndTargetValue (1.0f);
    type = previousType = settings.type;

    updateCoefficients();
    reset();
}

void FilterStage::setTargets (const Settings& settings)
{
    cutoff.setTargetValue (limitCutoff (settings.cutoff));
    q.setTargetValue (getQ (settings));
    mix.setTargetValue (settings.on ? 1.0f : 0.0f);

    // A new type fades in from the old one.
    if (settings.type != type)
    {
        previousType = type;
        type = settings.type;
        typeMix.setCurrentAndTargetValue (0.0f);
        typeMix.setTargetValue (1.0f);
    }
}

bool FilterStage::isActive() const noexcept
{
    return mix.getTargetValue() > 0.0f || mix.isSmoothing();
}

void FilterStage::updateCoefficients()
{
    // The topology-preserving transform of the state variable filter (Zavalishin, The Art of VA Filter Design).
    g = (float) std::tan (juce::MathConstants<double>::pi * cutoff.getCurrentValue() / sampleRate);
    r2 = 1.0f / q.getCurrentValue();
    h = 1.0f / (1.0f + r2 * g + g * g);
}

float FilterStage::process (float input)
{
    if (cutoff.isSmoothing() || q.isSmoothing())
    {
        cutoff.getNextValue();
        q.getNextValue();
        updateCoefficients();
    }

    const auto highPass = (input - (r2 + g) * s1 - s2) * h;
    const auto bandPass = g * highPass + s1;
    s1 = g * highPass + bandPass;
    const auto lowPass = g * bandPass + s2;
    s2 = g * bandPass + lowPass;

    // The band is scaled down by its Q, so a mid pass lets its middle through at the level it came in, however
    // narrow it is, rather than boosting it.
    const auto output = [&] (FilterType outputType)
    {
        switch (outputType)
        {
            case FilterType::lowPass:   return lowPass;
            case FilterType::midPass:   return r2 * bandPass;
            case FilterType::highPass:  return highPass;
        }

        return lowPass;
    };

    const auto typeShare = typeMix.getNextValue();
    const auto filtered = typeShare >= 1.0f ? output (type) : output (previousType) + typeShare * (output (type) - output (previousType));

    // Switching the filter in or out fades between the filtered sound and the sound as it was.
    return input + mix.getNextValue() * (filtered - input);
}

void FilterStage::reset()
{
    s1 = s2 = 0.0f;
}
