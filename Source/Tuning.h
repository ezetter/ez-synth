#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>

/** Tuning an oscillator up from the note played, by anything up to an octave. */
namespace tuning
{
    /** The furthest an oscillator can be tuned up, in semitones. */
    inline constexpr float octave = 12.0f;

    /** An interval up from the note, by its number, such as a 5th, and how many semitones it spans. */
    struct Interval
    {
        int number = 1;
        int semitones = 0;
    };

    /** The intervals marked along the tune slider: the major 2nd, 3rd, 6th and 7th, and the perfect 4th and 5th. */
    inline constexpr std::array<Interval, 6> markedIntervals { { { 2, 2 }, { 3, 4 }, { 4, 5 }, { 5, 7 }, { 6, 9 }, { 7, 11 } } };

    /** How close to a whole number of semitones dragging the tune slider snaps to it. */
    inline constexpr double snapDistance = 0.1;

    /** A tuning moved to the nearest whole number of semitones, if it's within the snapping distance of it. */
    double snap (double semitones);

    /** The whole number of semitones a tuning is, if it's one. */
    std::optional<int> toWholeSemitones (float semitones);

    /** How far an oscillator is tuned up, such as "+7 st" or "+0.15 st". */
    juce::String toText (float semitones);

    /** The interval a whole number of semitones up makes, from "unison" to "octave", such as "perfect 5th". */
    juce::String getIntervalName (int semitones);

    /** How many times the note's frequency an oscillator tuned this many semitones up plays at. */
    double toRatio (double semitones);
}
