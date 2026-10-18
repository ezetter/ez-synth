#pragma once

#include <juce_dsp/juce_dsp.h>

/** A low pass or high pass filter, 12 dB an octave, that can be switched in and out, swept and made resonant
    without clicks: every change glides over a few milliseconds.
*/
class FilterStage
{
public:
    explicit FilterStage (juce::dsp::StateVariableTPTFilterType);

    static constexpr float lowestCutoff = 20.0f;
    static constexpr float highestCutoff = 20000.0f;

    /** How sharply the filter peaks at its cutoff for a resonance from 0 to 1: from no peak at all, a Q of 0.71,
        up to a Q of 8.
    */
    static float resonanceToQ (float resonance);

    /** Where a cutoff sits along its dial, from 0 to 1, and back. Cutoffs are spread evenly by octave, as pitch is
        heard.
    */
    static float cutoffToPosition (float cutoff);
    static float positionToCutoff (float position);

    /** Gets ready to play, starting with these settings rather than gliding to them. */
    void prepare (const juce::dsp::ProcessSpec&, bool on, float cutoff, float resonance);

    /** Goes straight to these settings, forgetting the sound so far, as when a voice starts a new note. */
    void jumpTo (bool on, float cutoff, float resonance);

    /** Sets the settings to glide to. Cutoffs too near the Nyquist frequency are lowered, as the filter can't
        work there.
    */
    void setTargets (bool on, float cutoff, float resonance);

    /** Whether the filter is on, or still fading out, so the sound needs to go through it. */
    bool isActive() const noexcept;

    /** Moves the settings one sample along their glides. Call this once for each sample, before processing the
        sample on each channel.
    */
    void advance();

    float process (int channel, float input);

    /** Clears the filter's memory of the sound, as when it's switched out. */
    void reset();

private:
    float limitCutoff (float) const;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff;
    juce::SmoothedValue<float> q, mix;
    double sampleRate = 44100.0;
    float currentMix = 0.0f;
};
