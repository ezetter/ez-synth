#include "FilterStage.h"

namespace
{
    /** How much of a band a notch takes away for a depth in decibels: all of it for an infinite depth. */
    float depthToCut (float decibels)
    {
        return 1.0f - juce::Decibels::decibelsToGain (-juce::jmax (0.0f, decibels), -1000.0f);
    }
}

juce::String FilterStage::getName (FilterType type)
{
    switch (type)
    {
        case FilterType::lowPass:   return "Low pass";
        case FilterType::midPass:   return "Mid pass";
        case FilterType::highPass:  return "High pass";
        case FilterType::notch:     return "Notch";
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
        case FilterType::notch:     return "Notch";
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

const juce::NormalisableRange<float>& FilterStage::qRange()
{
    static const auto range = []
    {
        juce::NormalisableRange<float> result (0.5f, 20.0f);
        result.setSkewForCentre (3.0f);
        return result;
    }();

    return range;
}

float FilterStage::qToOctaves (float q)
{
    return 2.0f / std::log (2.0f) * std::asinh (1.0f / (2.0f * q));
}

const juce::NormalisableRange<float>& FilterStage::depthRange()
{
    static const juce::NormalisableRange<float> range (0.0f, 60.0f);
    return range;
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
    switch (settings.type)
    {
        case FilterType::midPass:   return widthToQ (settings.width);
        case FilterType::notch:     return juce::jlimit (qRange().start, qRange().end, settings.q);
        case FilterType::lowPass:
        case FilterType::highPass:  break;
    }

    return resonanceToQ (settings.resonance);
}

void FilterStage::prepare (double newSampleRate, const Settings& settings)
{
    sampleRate = newSampleRate;

    // Quick enough to follow an LFO at 100 Hz, slow enough not to click.
    cutoff.reset (sampleRate, 0.0015);
    q.reset (sampleRate, 0.0015);
    mix.reset (sampleRate, 0.02);
    typeMix.reset (sampleRate, 0.02);
    notchCut.reset (sampleRate, 0.0015);

    jumpTo (settings);
}

void FilterStage::jumpTo (const Settings& settings)
{
    cutoff.setCurrentAndTargetValue (limitCutoff (settings.cutoff));
    q.setCurrentAndTargetValue (getQ (settings));
    mix.setCurrentAndTargetValue (settings.on ? 1.0f : 0.0f);
    typeMix.setCurrentAndTargetValue (1.0f);
    notchCut.setCurrentAndTargetValue (depthToCut (settings.depth));
    type = previousType = settings.type;

    updateCoefficients();
}

void FilterStage::setTargets (const Settings& settings)
{
    cutoff.setTargetValue (limitCutoff (settings.cutoff));
    q.setTargetValue (getQ (settings));
    mix.setTargetValue (settings.on ? 1.0f : 0.0f);
    notchCut.setTargetValue (depthToCut (settings.depth));

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

FilterStage::Step FilterStage::advance()
{
    if (cutoff.isSmoothing() || q.isSmoothing())
    {
        cutoff.getNextValue();
        q.getNextValue();
        updateCoefficients();
    }

    Step step;
    step.cut = notchCut.getNextValue();
    step.typeShare = typeMix.getNextValue();
    step.wet = mix.getNextValue();
    return step;
}

float FilterStage::filter (float input, Memory& memory, const Step& step, float sent) const
{
    const auto highPass = (input - (r2 + g) * memory.s1 - memory.s2) * h;
    const auto bandPass = g * highPass + memory.s1;
    memory.s1 = g * highPass + bandPass;
    const auto lowPass = g * bandPass + memory.s2;
    memory.s2 = g * bandPass + lowPass;

    // The band is scaled down by its Q, so a mid pass lets its middle through at the level it came in, however
    // narrow it is, rather than boosting it. A notch takes that band away from the sound, all of it at full depth,
    // which leaves the low and high passes added together: the standard notch, (s² + 1) / (s² + s / Q + 1).
    const auto band = r2 * bandPass;

    const auto output = [&] (FilterType outputType)
    {
        switch (outputType)
        {
            case FilterType::lowPass:   return lowPass;
            case FilterType::midPass:   return band;
            case FilterType::highPass:  return highPass;
            case FilterType::notch:     return input - step.cut * band;
        }

        return lowPass;
    };

    const auto filtered = step.typeShare >= 1.0f ? output (type)
                                                 : output (previousType) + step.typeShare * (output (type) - output (previousType));

    // Switching the filter in or out, or sending the sound through it, fades between the filtered sound and the sound
    // as it was.
    return input + step.wet * sent * (filtered - input);
}
