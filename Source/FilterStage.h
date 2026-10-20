#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/** What a filter lets through: the frequencies below its cutoff, a band around it, or those above it. */
enum class FilterType { lowPass, midPass, highPass };

/** A filter, 12 dB an octave, that's a low pass, a mid pass or a high pass, and that can be switched in and out,
    changed from one type to another, swept, made resonant or wider without clicks: every change glides over a few
    milliseconds. It's a state variable filter, which works out all three types at once, so changing type fades from
    one to the other.
*/
class FilterStage
{
public:
    static constexpr float lowestCutoff = 20.0f;
    static constexpr float highestCutoff = 20000.0f;

    /** What each type is called, such as "Low pass", and called on its button, such as "Low". */
    static juce::String getName (FilterType);
    static juce::String getShortName (FilterType);

    /** How sharply a low or high pass peaks at its cutoff for a resonance from 0 to 1: from no peak at all, a Q of
        0.71, up to a Q of 8.
    */
    static float resonanceToQ (float resonance);

    /** How wide a mid pass's band can be, in octaves between the points where it lets through half the power, from
        narrow to wide, and the Q that makes a band that wide.
    */
    static const juce::NormalisableRange<float>& widthRange();
    static float widthToQ (float octaves);

    /** Where a cutoff sits along its dial, from 0 to 1, and back. Cutoffs are spread evenly by octave, as pitch is
        heard.
    */
    static float cutoffToPosition (float cutoff);
    static float positionToCutoff (float position);

    /** A filter's settings. A low or high pass uses its resonance; a mid pass, whose cutoff is the middle of its
        band, uses its width.
    */
    struct Settings
    {
        bool on = false;
        FilterType type = FilterType::lowPass;
        float cutoff = 1000.0f;
        float resonance = 0.0f;
        float width = 2.0f;
    };

    /** Gets ready to play at a sample rate, starting with these settings rather than gliding to them. */
    void prepare (double sampleRate, const Settings&);

    /** Goes straight to these settings, forgetting the sound so far, as when a voice starts a new note. */
    void jumpTo (const Settings&);

    /** Sets the settings to glide to. Cutoffs too near the Nyquist frequency are lowered, as the filter can't
        work there.
    */
    void setTargets (const Settings&);

    /** Whether the filter is on, or still fading out, so the sound needs to go through it. */
    bool isActive() const noexcept;

    /** Filters a sample, moving the settings one sample along their glides. */
    float process (float input);

    /** Clears the filter's memory of the sound, as when it's switched out. */
    void reset();

private:
    float limitCutoff (float) const;
    float getQ (const Settings&) const;
    void updateCoefficients();

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff, q;
    juce::SmoothedValue<float> mix, typeMix;
    FilterType type = FilterType::lowPass, previousType = FilterType::lowPass;
    double sampleRate = 44100.0;

    // The state variable filter: its two integrators, and its coefficients.
    float s1 = 0.0f, s2 = 0.0f;
    float g = 0.0f, r2 = 1.0f, h = 1.0f;
};
