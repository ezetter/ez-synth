#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>

/** Tuning an oscillator away from the note played, by anything up to an octave below or above it. */
namespace tuning
{
    /** The furthest an oscillator can be tuned down and up, in semitones. */
    inline constexpr float lowest = -12.0f;
    inline constexpr float highest = 12.0f;

    /** Where a tuning lies along the tune slider's travel, from 0 an octave down to 1 an octave up, as LFOs,
        envelopes and velocity move it.
    */
    float toPosition (float semitones);
    float fromPosition (float position);

    /** An interval from the note, by its number, such as a 5th, and how many semitones it spans. */
    struct Interval
    {
        int number = 1;
        int semitones = 0;
    };

    /** The intervals marked along the tune slider, above the note and below it: the major 2nd, 3rd, 6th and 7th,
        the perfect 4th and 5th, and the octave.
    */
    inline constexpr std::array<Interval, 7> markedIntervals { { { 2, 2 }, { 3, 4 }, { 4, 5 }, { 5, 7 }, { 6, 9 }, { 7, 11 }, { 8, 12 } } };

    /** A tuning moved to the nearest whole number of semitones, if it's within the given distance of it. */
    double snap (double semitones, double distance);

    /** The whole number of semitones a tuning is, if it's one. */
    std::optional<int> toWholeSemitones (float semitones);

    /** How far an oscillator is tuned, such as "+7 st", "−0.15 st" or "0 st". */
    juce::String toText (float semitones);

    /** The interval a whole number of semitones makes, such as "perfect 5th", or "perfect 5th below" for -7. */
    juce::String getIntervalName (int semitones);

    /** How many times the note's frequency an oscillator tuned this many semitones plays at. */
    double toRatio (double semitones);

    //==============================================================================
    /** How much of the tune slider's scale is shown: from an octave either side of the note, zoomed right out, to
        a semitone either side, zoomed right in.
    */
    inline constexpr float widestReach = 12.0f;
    inline constexpr float narrowestReach = 1.0f;

    /** How far either side of its middle the scale reaches when zoomed in by this much, from 0, all the way out, to
        1, all the way in. Each step in zooms in by the same factor.
    */
    float zoomToReach (float zoom);

    /** The part of the scale shown, from its lowest to its highest tuning. */
    struct Window
    {
        float low = lowest;
        float high = highest;

        bool contains (float semitones) const noexcept   { return semitones >= low && semitones <= high; }
    };

    /** The part of the scale to show, reaching as far as given either side of the whole semitone nearest a tuning,
        but kept within the octave either way. Zoomed in from the note itself, it's the note's semitone either side.
    */
    Window getWindow (float semitones, float reach);
}
