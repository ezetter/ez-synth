#include "FilterStage.h"

FilterStage::FilterStage (juce::dsp::StateVariableTPTFilterType type)
{
    filter.setType (type);
}

float FilterStage::resonanceToQ (float resonance)
{
    return juce::MathConstants<float>::sqrt2 / 2.0f * std::pow (2.0f, 3.5f * juce::jlimit (0.0f, 1.0f, resonance));
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

void FilterStage::prepare (const juce::dsp::ProcessSpec& spec, bool on, float newCutoff, float resonance)
{
    sampleRate = spec.sampleRate;
    filter.prepare (spec);

    // Quick enough to follow an LFO, slow enough not to click.
    cutoff.reset (sampleRate, 0.01);
    q.reset (sampleRate, 0.01);
    mix.reset (sampleRate, 0.02);

    jumpTo (on, newCutoff, resonance);
}

void FilterStage::jumpTo (bool on, float newCutoff, float resonance)
{
    cutoff.setCurrentAndTargetValue (limitCutoff (newCutoff));
    q.setCurrentAndTargetValue (resonanceToQ (resonance));
    mix.setCurrentAndTargetValue (on ? 1.0f : 0.0f);
    currentMix = mix.getCurrentValue();

    filter.setCutoffFrequency (cutoff.getCurrentValue());
    filter.setResonance (q.getCurrentValue());
    filter.reset();
}

void FilterStage::setTargets (bool on, float newCutoff, float resonance)
{
    cutoff.setTargetValue (limitCutoff (newCutoff));
    q.setTargetValue (resonanceToQ (resonance));
    mix.setTargetValue (on ? 1.0f : 0.0f);
}

bool FilterStage::isActive() const noexcept
{
    return mix.getTargetValue() > 0.0f || mix.isSmoothing();
}

void FilterStage::advance()
{
    if (cutoff.isSmoothing())
        filter.setCutoffFrequency (cutoff.getNextValue());

    if (q.isSmoothing())
        filter.setResonance (q.getNextValue());

    currentMix = mix.getNextValue();
}

float FilterStage::process (int channel, float input)
{
    // Switching the filter in or out fades between the filtered sound and the sound as it was.
    return input + currentMix * (filter.processSample (channel, input) - input);
}

void FilterStage::reset()
{
    filter.reset();
}
