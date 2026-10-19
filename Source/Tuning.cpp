#include "Tuning.h"

namespace tuning
{
    double snap (double semitones)
    {
        const auto nearest = std::round (semitones);
        return std::abs (semitones - nearest) <= snapDistance ? nearest : semitones;
    }

    std::optional<int> toWholeSemitones (float semitones)
    {
        const auto nearest = juce::roundToInt (semitones);
        return std::abs (semitones - (float) nearest) < 0.005f ? std::optional (nearest) : std::nullopt;
    }

    juce::String toText (float semitones)
    {
        if (const auto whole = toWholeSemitones (semitones))
            return (*whole > 0 ? "+" : "") + juce::String (*whole) + " st";

        return "+" + juce::String (semitones, 2) + " st";
    }

    juce::String getIntervalName (int semitones)
    {
        static const std::array<const char*, 13> names { "unison", "minor 2nd", "major 2nd", "minor 3rd", "major 3rd",
                                                         "perfect 4th", "tritone", "perfect 5th", "minor 6th", "major 6th",
                                                         "minor 7th", "major 7th", "octave" };

        return names[(size_t) juce::jlimit (0, (int) names.size() - 1, semitones)];
    }

    double toRatio (double semitones)
    {
        return std::pow (2.0, semitones / 12.0);
    }
}
